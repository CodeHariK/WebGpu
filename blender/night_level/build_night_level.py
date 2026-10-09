"""Hollow Isle — builds a cartoony, spooky night level in Blender from night_map.json.

Run inside Blender (Scripting tab, or exec(open(path).read())). It clears the scene and builds:
  Terrain     floating islands (flat grassy tops, rocky cliffs tapering to a point), the ramp
  Bridges     arched plank bridges with wobbly railings
  Platforms   moving platforms (keyframed ping-pong, looping) — custom props: move_from/to, period
  Props       crooked houses with glowing windows, dead trees, lamp posts, gravestones, the tower
  Crystals    glowing crystal clusters — custom prop item = "crystal"
  Spawns      empties: EnemySpawn_* (enemy, count, radius) and PlayerStart
  Sky         fog sea, a big moon, moonlight, lantern lights
Custom properties export to glTF as node extras, so Godot gets them as metadata.
Everything is seeded: the same map gives the same level.
"""
import bpy
import bmesh
import json
import math
import os
import random
from mathutils import Vector, Matrix, Euler

HERE = os.path.dirname(os.path.abspath(__file__)) if "__file__" in globals() else "/Users/Shared/Code/WebGpu/blender/night_level"
MAP_PATH = os.path.join(HERE, "night_map.json")

PALETTE = {
    "grass": (0.20, 0.27, 0.40), "rock": (0.20, 0.17, 0.24), "path": (0.30, 0.27, 0.33),
    "wood": (0.25, 0.16, 0.12), "rail": (0.06, 0.05, 0.08), "wall": (0.42, 0.38, 0.50),
    "roof": (0.12, 0.10, 0.20), "door": (0.05, 0.03, 0.05), "window": (1.0, 0.72, 0.25),
    "bark": (0.10, 0.08, 0.10), "stone": (0.36, 0.36, 0.42), "crystal": (0.25, 0.95, 1.0),
    "lamp": (1.0, 0.75, 0.35), "fog": (0.022, 0.022, 0.045), "moon": (1.0, 0.85, 0.45),
    "platform": (0.30, 0.22, 0.38), "spawn": (1.0, 0.1, 0.1),
}


# --- scene and materials -----------------------------------------------------------------------

def clear_scene():
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    for coll in list(bpy.data.collections):
        bpy.data.collections.remove(coll)
    for block in (bpy.data.meshes, bpy.data.materials, bpy.data.lights, bpy.data.cameras, bpy.data.actions):
        for item in list(block):
            if item.users == 0:
                block.remove(item)


def collection(name):
    coll = bpy.data.collections.new(name)
    bpy.context.scene.collection.children.link(coll)
    return coll


_materials = {}


def material(key, emission=0.0):
    name = f"M_{key}" + ("_glow" if emission else "")
    if name in _materials:
        return _materials[name]
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    color = PALETTE[key]
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    bsdf.inputs["Roughness"].default_value = 0.85
    if emission:
        bsdf.inputs["Emission Color"].default_value = (*color, 1.0)
        bsdf.inputs["Emission Strength"].default_value = emission
    mat.diffuse_color = (*color, 1.0) # viewport solid colour
    _materials[name] = mat
    return mat


def unlit_material(key):
    """Just a colour, no lighting or shadows (the fog sea: a flat dark backdrop)."""
    mat = bpy.data.materials.new(f"M_{key}_unlit")
    mat.use_nodes = True
    nodes = mat.node_tree.nodes
    nodes.clear()
    emit = nodes.new("ShaderNodeEmission")
    emit.inputs["Color"].default_value = (*PALETTE[key], 1.0)
    out = nodes.new("ShaderNodeOutputMaterial")
    mat.node_tree.links.new(emit.outputs["Emission"], out.inputs["Surface"])
    mat.diffuse_color = (*PALETTE[key], 1.0)
    return mat


def mesh_object(name, bm, coll, materials, smooth=False):
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    for mat in materials:
        mesh.materials.append(mat)
    for poly in mesh.polygons:
        poly.use_smooth = smooth
    obj = bpy.data.objects.new(name, mesh)
    coll.objects.link(obj)
    return obj


def box(bm, center, size, mat_index=0, rot_z=0.0, tilt=(0.0, 0.0)):
    """Add a box (centred on `center`) to bm: pitched / rolled by tilt, then turned by rot_z."""
    m = Matrix.Translation(Vector(center)) @ Euler((tilt[0], tilt[1], rot_z)).to_matrix().to_4x4() @ Matrix.Diagonal((*size, 1.0))
    result = bmesh.ops.create_cube(bm, size=1.0, matrix=m)
    for f in {f for v in result["verts"] for f in v.link_faces}:
        f.material_index = mat_index
    return result


def cylinder_between(bm, a, b, r_a, r_b, segments=6, mat_index=0):
    """A tapered cylinder from point a to point b (a twisted branch, a post, a drum)."""
    a, b = Vector(a), Vector(b)
    span = b - a
    rot = Vector((0, 0, 1)).rotation_difference(span.normalized()).to_matrix().to_4x4()
    m = Matrix.Translation((a + b) / 2) @ rot
    result = bmesh.ops.create_cone(bm, cap_ends=True, segments=segments, radius1=r_a, radius2=r_b, depth=span.length, matrix=m)
    for f in {f for v in result["verts"] for f in v.link_faces}:
        f.material_index = mat_index


def tag(obj, **props):
    """Custom properties: they go into the glTF as node extras (Godot metadata)."""
    for key, value in props.items():
        obj[key] = value


def to_godot(v):
    """Blender (x, y, z up) → Godot (x, y up, −z forward)."""
    return [round(v[0], 3), round(v[2], 3), round(-v[1], 3)]


# --- the map -----------------------------------------------------------------------------------

class Level:
    def __init__(self, data):
        self.data = data
        self.heights = {i["name"]: i["height"] for i in data["islands"]}

    def ground(self, item):
        """Height an item stands on: its 'z', or the top of the island named in 'on'."""
        if "z" in item:
            return item["z"]
        return self.heights.get(item.get("on", ""), 0.0)

    def point(self, item, lift=0.0):
        return Vector((item["at"][0], item["at"][1], self.ground(item) + lift))


# --- terrain -------------------------------------------------------------------------------------

def island_outline(island, segments=48):
    rng = random.Random(island["seed"])
    a, b, c = rng.uniform(0, 6.28), rng.uniform(0, 6.28), rng.uniform(0, 6.28)
    cx, cy = island["center"]
    points = []
    for i in range(segments):
        t = i / segments * math.tau
        wobble = 1.0 + 0.10 * math.sin(3 * t + a) + 0.06 * math.sin(5 * t + b) + 0.03 * math.sin(9 * t + c)
        if island["shape"] == "square": # a rounded rectangle (superellipse) with a little wobble
            hx, hy = island["size"][0] / 2, island["size"][1] / 2
            ct, st = math.cos(t), math.sin(t)
            x = hx * math.copysign(abs(ct) ** 0.25, ct)
            y = hy * math.copysign(abs(st) ** 0.25, st)
            wobble = 1.0 + (wobble - 1.0) * 0.25
            points.append((cx + x * wobble, cy + y * wobble))
        else:
            r = island["radius"] * wobble
            points.append((cx + math.cos(t) * r, cy + math.sin(t) * r))
    return points


def build_island(island, coll):
    """Flat grass top, a short straight cliff, then rock tapering down to a point (a floating island)."""
    bm = bmesh.new()
    outline = island_outline(island)
    cx, cy = island["center"]
    h, depth = island["height"], island["depth"]
    rings = [(1.0, 0.0), (1.0, -0.5), (0.86, -depth * 0.35), (0.55, -depth * 0.7)]
    rng = random.Random(island["seed"] * 7)
    ring_verts = []
    for scale, dz in rings:
        jitter = 0.0 if dz > -1 else 0.25
        ring = []
        for x, y in outline:
            px = cx + (x - cx) * scale + rng.uniform(-jitter, jitter)
            py = cy + (y - cy) * scale + rng.uniform(-jitter, jitter)
            ring.append(bm.verts.new((px, py, h + dz)))
        ring_verts.append(ring)
    tip = bm.verts.new((cx + rng.uniform(-1, 1), cy + rng.uniform(-1, 1), h - depth))
    top = bm.faces.new(ring_verts[0])
    top.material_index = 0
    n = len(outline)
    for upper, lower in zip(ring_verts, ring_verts[1:]):
        for i in range(n):
            f = bm.faces.new((upper[i], upper[(i + 1) % n], lower[(i + 1) % n], lower[i]))
            f.material_index = 1
    for i in range(n):
        f = bm.faces.new((ring_verts[-1][i], ring_verts[-1][(i + 1) % n], tip))
        f.material_index = 1
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    obj = mesh_object(f"Island_{island['name']}-col", bm, coll, [material("grass"), material("rock")])
    tag(obj, kind="terrain", island=island["name"])
    return obj


def build_ramp(path, coll):
    """A sloped stone slab from 'from' (at height[0]) to 'to' (at height[1])."""
    a = Vector((*path["from"], path["height"][0]))
    b = Vector((*path["to"], path["height"][1]))
    flat = Vector((b.x - a.x, b.y - a.y, 0)).normalized()
    side = Vector((-flat.y, flat.x, 0)) * path["width"] / 2
    thick = Vector((0, 0, 0.8))
    bm = bmesh.new()
    corners = [a - side, a + side, b + side, b - side]
    top = [bm.verts.new(p) for p in corners]
    bottom = [bm.verts.new(p - thick) for p in corners]
    bm.faces.new(top)
    bm.faces.new(list(reversed(bottom)))
    for i in range(4):
        bm.faces.new((top[i], bottom[i], bottom[(i + 1) % 4], top[(i + 1) % 4]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    obj = mesh_object(f"Path_{path['name']}-col", bm, coll, [material("path")])
    tag(obj, kind="path")


# --- bridges and platforms -------------------------------------------------------------------------

def build_bridge(bridge, coll):
    """Wooden planks along an arch (with small gaps), wobbly railing posts and rails, end supports."""
    rng = random.Random(sum(map(ord, bridge["name"])))
    a2, b2 = Vector(bridge["from"]), Vector(bridge["to"])
    length = (b2 - a2).length
    yaw = math.atan2(b2.y - a2.y, b2.x - a2.x)
    side = Vector((-math.sin(yaw), math.cos(yaw), 0))
    planks = max(int(length / 0.75), 4)
    h0, h1, arch, width = bridge["height"][0], bridge["height"][1], bridge["arch"], bridge["width"]

    def at(t):
        p = a2.lerp(b2, t)
        return Vector((p.x, p.y, h0 + (h1 - h0) * t + arch * math.sin(math.pi * t)))

    deck = bmesh.new()
    for i in range(planks):
        t0, t1 = i / planks, (i + 1) / planks
        p0, p1 = at(t0), at(t1)
        mid = (p0 + p1) / 2 - Vector((0, 0, 0.12))
        slope = math.atan2(p1.z - p0.z, (p1 - p0).xy.length)
        box(deck, mid, ((p1 - p0).length * 0.88, width * rng.uniform(0.92, 1.0), 0.24), 0, yaw, (0.0, -slope))
    obj = mesh_object(f"Bridge_{bridge['name']}-col", deck, coll, [material("wood")])
    tag(obj, kind="bridge")

    rails = bmesh.new()
    posts = []
    for i in range(0, planks + 1, 2):
        t = i / planks
        for s in (-1, 1):
            base = at(t) + side * s * (width / 2 - 0.1)
            top = base + Vector((rng.uniform(-0.08, 0.08), rng.uniform(-0.08, 0.08), 1.0 + rng.uniform(-0.1, 0.1)))
            cylinder_between(rails, base, top, 0.07, 0.05, 5)
            posts.append((s, top))
    for s in (-1, 1):
        tops = [p for side_, p in posts if side_ == s]
        for p, q in zip(tops, tops[1:]):
            cylinder_between(rails, p, q, 0.045, 0.045, 4)
    for t in (0.0, 1.0): # supports into the rock at both ends
        for s in (-1, 1):
            top = at(t) + side * s * (width / 2 - 0.2) - Vector((0, 0, 0.2))
            cylinder_between(rails, top, top - Vector((0, 0, 4)), 0.16, 0.12, 6)
    railing = mesh_object(f"Bridge_{bridge['name']}_Rails", rails, coll, [material("rail")])
    tag(railing, kind="bridge_rail")


def build_platform(platform, coll, fps):
    """A slab that ping-pongs between 'from' and 'to' (eased at the ends), looping forever."""
    sx, sy = platform["size"]
    bm = bmesh.new()
    box(bm, (0, 0, -0.3), (sx, sy, 0.6), 0)
    box(bm, (0, 0, -0.65), (sx * 0.6, sy * 0.6, 0.2), 1) # a glowing underside rune
    obj = mesh_object(f"Platform_{platform['name']}", bm, coll, [material("platform"), material("crystal", 2.0)])
    a = Vector((*platform["from"], platform["z"]))
    b = Vector((*platform["to"], platform["z"]))
    period = platform["period"] * fps
    start = 1 - platform.get("phase", 0.0) * period
    for frame, where in ((start, a), (start + period / 2, b), (start + period, a)):
        obj.location = where
        obj.keyframe_insert("location", frame=frame)
    obj.location = a
    if obj.animation_data and obj.animation_data.action:
        action = obj.animation_data.action
        curves = []
        try:
            curves = list(action.fcurves)
        except AttributeError: # Blender 5 layered actions
            for layer in action.layers:
                for strip in layer.strips:
                    for bag in strip.channelbags:
                        curves.extend(bag.fcurves)
        for fc in curves:
            fc.modifiers.new("CYCLES")
    tag(obj, kind="moving_platform", move_from=to_godot(a), move_to=to_godot(b), period=platform["period"], phase=platform.get("phase", 0.0))
    return obj


# --- props ---------------------------------------------------------------------------------------

def build_house(house, level, coll, index):
    """A crooked little house: leaning walls, a tall bent roof, a chimney, glowing windows, a door."""
    rng = random.Random(100 + index)
    w, d, h = house["size"]
    bm = bmesh.new()
    box(bm, (0, 0, h / 2), (w, d, h), 0)
    # Roof: a pyramid with its tip pushed sideways (crooked), overhanging the walls.
    over = 0.35
    base = [bm.verts.new((sx * (w / 2 + over), sy * (d / 2 + over), h)) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    tip = bm.verts.new((rng.uniform(-0.3, 0.3) * w, rng.uniform(-0.3, 0.3) * d, h + w * rng.uniform(0.9, 1.3)))
    for i in range(4):
        bm.faces.new((base[i], base[(i + 1) % 4], tip)).material_index = 1
    bm.faces.new(list(reversed(base))).material_index = 1
    box(bm, (w * 0.25, d * 0.1, h + w * 0.45), (0.6, 0.6, w * 0.8), 1, 0.0, (0.0, math.radians(8))) # chimney
    box(bm, (0, -d / 2 - 0.03, 1.0), (1.0, 0.12, 2.0), 2) # door
    for side, (nx, ny) in enumerate(((0, -1), (1, 0), (-1, 0), (0, 1))):
        across = w if ny else d
        out = d / 2 if ny else w / 2
        rows = 2 if h > 5 else 1
        for row in range(rows):
            for col in (-1, 1):
                if side == 0 and row == 0:
                    continue # the door's wall: windows upstairs only
                z = 2.6 + row * 2.0
                along = col * across * 0.25
                pos = (nx * (out + 0.03) + (along if ny else 0), ny * (out + 0.03) + (0 if ny else along), z)
                size = (0.7, 0.1, 0.9) if ny else (0.1, 0.7, 0.9)
                box(bm, pos, size, 3)
    obj = mesh_object(f"House_{index}-col", bm, coll, [material("wall"), material("roof"), material("door"), material("window", 4.0)])
    obj.location = level.point(house)
    obj.rotation_euler = (math.radians(house.get("lean", 0)), 0, math.radians(house.get("rot", 0)))
    tag(obj, kind="house")


def build_tree(tree, level, coll, index):
    """A dead, twisted tree: a bent trunk of tapering segments and a few crooked branches."""
    rng = random.Random(200 + index)
    bm = bmesh.new()
    p = Vector((0, 0, -0.3))
    direction = Vector((0, 0, 1))
    radius = 0.45
    joints = []
    for _ in range(5):
        direction = (direction + Vector((rng.uniform(-0.45, 0.45), rng.uniform(-0.45, 0.45), 0.25))).normalized()
        q = p + direction * rng.uniform(1.1, 1.6)
        cylinder_between(bm, p, q, radius, radius * 0.75, 6)
        joints.append(q)
        p, radius = q, radius * 0.75
    for joint in joints[1:]:
        if rng.random() < 0.7:
            b_dir = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(0.1, 0.7))).normalized()
            end = joint + b_dir * rng.uniform(1.0, 2.2)
            cylinder_between(bm, joint, end, 0.13, 0.04, 5)
            twig = end + (b_dir + Vector((0, 0, 0.6))).normalized() * 0.7
            cylinder_between(bm, end, twig, 0.04, 0.015, 4)
    obj = mesh_object(f"Tree_{index}-col", bm, coll, [material("bark")])
    obj.location = level.point(tree)
    tag(obj, kind="tree")


def build_lamp(lamp, level, coll, light_coll, index):
    """A bent lamp post with a glowing lantern and a warm point light."""
    rng = random.Random(300 + index)
    bm = bmesh.new()
    top = Vector((rng.uniform(-0.15, 0.15), rng.uniform(-0.15, 0.15), 3.0))
    cylinder_between(bm, (0, 0, 0), top, 0.1, 0.07, 6)
    hook = top + Vector((0.6, 0, 0.1))
    cylinder_between(bm, top, hook, 0.05, 0.05, 4)
    lantern = hook - Vector((0, 0, 0.45))
    ball = bmesh.ops.create_icosphere(bm, subdivisions=1, radius=0.28, matrix=Matrix.Translation(lantern))
    for f in {f for v in ball["verts"] for f in v.link_faces}:
        f.material_index = 1
    obj = mesh_object(f"Lamp_{index}", bm, coll, [material("rail"), material("lamp", 6.0)])
    obj.location = level.point(lamp)
    obj.rotation_euler = (0, 0, rng.uniform(0, math.tau))
    light = bpy.data.lights.new(f"LampLight_{index}", "POINT")
    light.energy = 120
    light.color = PALETTE["lamp"]
    light.shadow_soft_size = 0.3
    lobj = bpy.data.objects.new(f"LampLight_{index}", light)
    light_coll.objects.link(lobj)
    lobj.location = obj.location + Euler((0, 0, obj.rotation_euler.z)).to_matrix() @ lantern
    tag(obj, kind="lamp")


def build_grave(grave, level, coll, index):
    rng = random.Random(400 + index)
    bm = bmesh.new()
    if rng.random() < 0.3: # a cross
        box(bm, (0, 0, 0.6), (0.18, 0.18, 1.4), 0)
        box(bm, (0, 0, 0.95), (0.8, 0.16, 0.16), 0)
    else: # a slab with a rounded top
        box(bm, (0, 0, 0.45), (0.8, 0.22, 0.9), 0)
        cylinder_between(bm, (0, -0.11, 0.9), (0, 0.11, 0.9), 0.4, 0.4, 10)
    obj = mesh_object(f"Grave_{index}-col", bm, coll, [material("stone")])
    obj.location = level.point(grave, -0.1)
    obj.rotation_euler = (math.radians(rng.uniform(-12, 12)), math.radians(rng.uniform(-10, 10)), math.radians(rng.uniform(-25, 25)))
    tag(obj, kind="grave")


def build_tower(tower, level, coll, light_coll):
    """The landmark: a leaning stack of drums narrowing upward, glowing windows, a lantern room and a
    bent witch-hat roof. Seen from everywhere, so the player always knows where to go."""
    rng = random.Random(500)
    bm = bmesh.new()
    drums = 6
    height, radius = tower["height"], tower["radius"]
    drum_h = height / drums
    centre = Vector((0, 0, 0))
    for i in range(drums):
        r0 = radius * (1.0 - 0.35 * i / drums)
        r1 = radius * (1.0 - 0.35 * (i + 1) / drums)
        lean = Vector((rng.uniform(0.15, 0.45), rng.uniform(-0.2, 0.2), 0)) # it leans one way, a bit more each drum
        top = centre + Vector((0, 0, drum_h)) + lean
        cylinder_between(bm, centre, top, r0, r1, 12, 0)
        for k in range(3): # windows round the drum
            ang = rng.uniform(0, math.tau) + k * math.tau / 3
            mid = centre.lerp(top, 0.55)
            r = (r0 + r1) / 2 + 0.05
            box(bm, mid + Vector((math.cos(ang) * r, math.sin(ang) * r, 0)), (0.12, 0.8, 1.2), 1, ang)
        centre = top
    lantern_at = centre + Vector((0, 0, 1.4))
    box(bm, lantern_at, (2.2, 2.2, 2.6), 1) # the lantern room (glows)
    hat_base = centre + Vector((0, 0, 2.7))
    tip = hat_base + Vector((1.6, 0.6, 5.5))
    cylinder_between(bm, hat_base, hat_base.lerp(tip, 0.5) + Vector((0.4, 0, 0)), radius * 0.9, radius * 0.35, 10, 2)
    cylinder_between(bm, hat_base.lerp(tip, 0.5) + Vector((0.4, 0, 0)), tip, radius * 0.35, 0.05, 10, 2)
    obj = mesh_object("Tower-col", bm, coll, [material("wall"), material("window", 5.0), material("roof")])
    obj.location = level.point(tower)
    tag(obj, kind="landmark")
    light = bpy.data.lights.new("TowerLight", "POINT")
    light.energy = 4000
    light.color = PALETTE["window"]
    light.shadow_soft_size = 1.0
    lobj = bpy.data.objects.new("TowerLight", light)
    light_coll.objects.link(lobj)
    lobj.location = obj.location + lantern_at


def build_crystal(crystal, level, coll, index):
    """A cluster of 3–4 long glowing crystals leaning outward (a collectible)."""
    rng = random.Random(600 + index)
    bm = bmesh.new()
    for k in range(rng.randint(3, 4)):
        height = rng.uniform(0.7, 1.3) * (1.4 if k == 0 else 1.0)
        width = height * 0.28
        ring = [bm.verts.new((math.cos(a) * width, math.sin(a) * width, height * 0.35)) for a in (0, math.tau / 3, 2 * math.tau / 3)]
        top = bm.verts.new((0, 0, height))
        bottom = bm.verts.new((0, 0, 0))
        for i in range(3):
            bm.faces.new((ring[i], ring[(i + 1) % 3], top))
            bm.faces.new((ring[(i + 1) % 3], ring[i], bottom))
        tilt = Euler((rng.uniform(-0.5, 0.5) if k else 0, rng.uniform(-0.5, 0.5) if k else 0, rng.uniform(0, math.tau))).to_matrix().to_4x4()
        bmesh.ops.transform(bm, matrix=tilt, verts=ring + [top, bottom])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    obj = mesh_object(f"Crystal_{index}", bm, coll, [material("crystal", 3.0)])
    obj.location = level.point(crystal)
    tag(obj, kind="pickup", item="crystal", value=1)


def build_spawn(spawn, level, coll, index):
    """An empty: where enemies appear. Its sphere shows the spawn radius."""
    obj = bpy.data.objects.new(f"EnemySpawn_{index}_{spawn['enemy']}", None)
    obj.empty_display_type = "SPHERE"
    obj.empty_display_size = max(spawn.get("radius", 1.0), 0.6)
    obj.location = level.point(spawn, 0.5)
    coll.objects.link(obj)
    marker = bpy.data.objects.new(f"EnemySpawn_{index}_arrow", None) # a tall arrow so it reads from far away
    marker.empty_display_type = "SINGLE_ARROW"
    marker.empty_display_size = 3.0
    marker.parent = obj
    coll.objects.link(marker)
    tag(obj, kind="enemy_spawn", enemy=spawn["enemy"], count=spawn.get("count", 1), radius=spawn.get("radius", 1.0))


def build_player_start(start, level, coll):
    obj = bpy.data.objects.new("PlayerStart", None)
    obj.empty_display_type = "ARROWS"
    obj.empty_display_size = 1.5
    obj.location = level.point(start, 0.1)
    obj.rotation_euler = (0, 0, math.radians(start.get("facing", 0)))
    coll.objects.link(obj)
    tag(obj, kind="player_start")


# --- sky ---------------------------------------------------------------------------------------

def build_sky(coll, light_coll):
    """A dark blue-violet night, a big low moon, cool moonlight and a sea of fog below the islands."""
    world = bpy.context.scene.world or bpy.data.worlds.new("World")
    bpy.context.scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    bg.inputs["Color"].default_value = (0.012, 0.014, 0.04, 1.0)
    bg.inputs["Strength"].default_value = 1.0
    bm = bmesh.new()
    bmesh.ops.create_uvsphere(bm, u_segments=32, v_segments=16, radius=30)
    moon = mesh_object("Moon", bm, coll, [material("moon", 3.0)], smooth=True)
    moon.location = (5, 260, 95)
    bm = bmesh.new()
    bmesh.ops.create_grid(bm, x_segments=1, y_segments=1, size=400)
    fog_mat = unlit_material("fog")
    fog = mesh_object("FogSea", bm, coll, [fog_mat])
    fog.location = (0, 40, -9)
    tag(fog, kind="kill_plane")
    sun = bpy.data.lights.new("Moonlight", "SUN")
    sun.energy = 2.0
    sun.color = (0.6, 0.65, 1.0)
    sobj = bpy.data.objects.new("Moonlight", sun)
    light_coll.objects.link(sobj)
    sobj.rotation_euler = (math.radians(55), 0, math.radians(200))


def build_camera(coll):
    cam = bpy.data.cameras.new("Overview")
    cam.lens = 28
    obj = bpy.data.objects.new("Overview", cam)
    coll.objects.link(obj)
    obj.location = (75, -45, 55)
    direction = Vector((5, 42, 0)) - obj.location
    obj.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.camera = obj


# --- main ----------------------------------------------------------------------------------------

def build(map_path=MAP_PATH):
    with open(map_path) as f:
        data = json.load(f)
    level = Level(data)
    fps = bpy.context.scene.render.fps
    clear_scene()
    _materials.clear()
    terrain, bridges, platforms = collection("Terrain"), collection("Bridges"), collection("Platforms")
    props, crystals, spawns = collection("Props"), collection("Crystals"), collection("Spawns")
    sky, lights = collection("Sky"), collection("Lights")
    for island in data["islands"]:
        build_island(island, terrain)
    for path in data.get("paths", []):
        build_ramp(path, terrain)
    for bridge in data.get("bridges", []):
        build_bridge(bridge, bridges)
    for platform in data.get("moving_platforms", []):
        build_platform(platform, platforms, fps)
    for i, house in enumerate(data.get("houses", [])):
        build_house(house, level, props, i)
    if "tower" in data:
        build_tower(data["tower"], level, props, lights)
    for i, tree in enumerate(data.get("trees", [])):
        build_tree(tree, level, props, i)
    for i, lamp in enumerate(data.get("lamps", [])):
        build_lamp(lamp, level, props, lights, i)
    for i, grave in enumerate(data.get("graves", [])):
        build_grave(grave, level, props, i)
    for i, crystal in enumerate(data.get("crystals", [])):
        build_crystal(crystal, level, crystals, i)
    for i, spawn in enumerate(data.get("spawns", [])):
        build_spawn(spawn, level, spawns, i)
    if "player_start" in data:
        build_player_start(data["player_start"], level, spawns)
    build_sky(sky, lights)
    build_camera(sky)
    longest = max([p["period"] for p in data.get("moving_platforms", [])] + [4.0])
    bpy.context.scene.frame_start = 1
    bpy.context.scene.frame_end = int(longest * fps)
    return {name: len(c.objects) for name, c in bpy.data.collections.items()}


def export(glb_path=os.path.join(HERE, "hollow_isle.glb")):
    bpy.ops.export_scene.gltf(filepath=glb_path, export_format="GLB", export_extras=True, export_lights=True, export_apply=True, export_animations=True)
    return glb_path


if __name__ == "__main__":
    print(build())
