"""Build papercraft.blend: a test of the low-poly "paper" style (a frog critter + a rock).

Run:  Blender -b --python make_papercraft.py      (saves papercraft.blend, preview.png and
      steps_preview.png here)

Step materials Paper_1_Flat … Paper_7_FoldLines each add ONE idea (frames in the node editor are
labelled with the step). The row of frogs behind the scene shows them; StepsCamera frames it.

The style is five simple ideas stacked in one material (PaperCraft node group inputs per material):
  1. LOW POLY, FLAT SHADED   blobby icospheres, jittered and decimated → uneven triangles, shade flat
  2. FACET TINT              every triangle gets a random number (Geometry Nodes "Facets" modifier
                             stores it as attribute "facet"); the shader nudges hue and brightness
                             by it, so neighbouring faces read as separate pieces of paper
  3. GRADIENT + WATERCOLOUR  colour ramp along the body (belly → back) + soft noise blotches of a
                             second colour, + a faint paper grain
  4. FOLD LINES              Wireframe node: thin pale lines on every triangle edge = paper folds
  5. BIG CARTOON EYES        not texture: flat discs (outline, white, iris, pupil, highlight)
"""
import math
import os
import random

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
random.seed(7)


# ---------------------------------------------------------------- materials

def facet_modifier_group():
    """Geometry Nodes: store one random number per FACE as attribute "facet"."""
    ng = bpy.data.node_groups.new("Facets", "GeometryNodeTree")
    ng.interface.new_socket("Geometry", in_out="INPUT", socket_type="NodeSocketGeometry")
    ng.interface.new_socket("Geometry", in_out="OUTPUT", socket_type="NodeSocketGeometry")
    gi, go = ng.nodes.new("NodeGroupInput"), ng.nodes.new("NodeGroupOutput")
    rnd = ng.nodes.new("FunctionNodeRandomValue")
    rnd.data_type = "FLOAT"
    store = ng.nodes.new("GeometryNodeStoreNamedAttribute")
    store.data_type, store.domain = "FLOAT", "FACE"
    store.inputs["Name"].default_value = "facet"
    value_out = next(o for o in rnd.outputs if o.enabled and o.type == "VALUE")
    value_in = next(i for i in store.inputs if i.enabled and i.type == "VALUE")
    ng.links.new(gi.outputs[0], store.inputs["Geometry"])
    ng.links.new(value_out, value_in)
    ng.links.new(store.outputs["Geometry"], go.inputs[0])
    return ng


STEP_NAMES = ["Flat", "Gradient", "Blotches", "Stripes", "FacetTint", "Grain", "FoldLines"]


def paper_material(name, ramp, axis="Z", span=(0.0, 1.0), blotch=(0.5, 0.2, 0.3), blotch_amount=0.45,
                   stripe=None, line=(0.92, 0.88, 0.8), line_size=0.007, steps=7):
    """The paper look, built up in 7 steps; `steps` stops after that many (for the step materials).
    ramp: [(pos, rgb)] along `axis` (object space, metres, mapped from span to 0..1).
    blotch: watercolour colour mixed in by soft noise. stripe: (rgb, bands) on top-facing faces.
    Each step's nodes sit in a frame labelled with the step, left to right in the node editor."""
    mat = bpy.data.materials.new(name)
    nt = mat.node_tree
    nt.nodes.clear()
    L = nt.links.new
    stage = {"nodes": []}

    def N(kind):
        n = nt.nodes.new(kind)
        stage["nodes"].append(n)
        return n

    def begin(k):
        stage["k"], stage["nodes"] = k, []

    def end():
        """Put this step's nodes in a labelled frame, stacked in their own column."""
        k = stage["k"]
        frame = nt.nodes.new("NodeFrame")
        frame.label = f"{k} · {STEP_NAMES[k - 1]}"
        frame.label_size = 20
        for i, n in enumerate(stage["nodes"]):
            n.location = (-2300 + k * 330, 300 - i * 190)
            n.parent = frame

    def mix(fac, a, b, blend="MIX"):
        m = N("ShaderNodeMix")
        m.data_type, m.blend_type = "RGBA", blend
        for sock, val in ((m.inputs[0], fac), (m.inputs[6], a), (m.inputs[7], b)):
            if isinstance(val, bpy.types.NodeSocket):
                L(val, sock)
            elif isinstance(val, (int, float)):
                sock.default_value = val
            else:
                sock.default_value = (*val, 1.0)
        return m.outputs[2]

    def color_ramp(fac, stops):
        r = N("ShaderNodeValToRGB")
        els = r.color_ramp.elements
        while len(els) < len(stops):
            els.new(0.5)
        for el, (pos, col) in zip(els, stops):
            el.position, el.color = pos, (*col, 1.0)
        L(fac, r.inputs["Fac"])
        return r.outputs["Color"]

    def scaled(value_socket, amount):
        m = N("ShaderNodeMath")
        m.operation = "MULTIPLY"
        L(value_socket, m.inputs[0])
        m.inputs[1].default_value = amount
        return m.outputs[0]

    # 1 · Flat: one colour (the middle of the ramp). Low poly + flat shading do the rest.
    begin(1)
    coords = N("ShaderNodeTexCoord").outputs["Object"]     # object space: pattern sticks to the model
    rgb = N("ShaderNodeRGB")
    rgb.outputs[0].default_value = (*ramp[len(ramp) // 2][1], 1.0)
    col = rgb.outputs[0]
    end()

    if steps >= 2:  # 2 · Gradient: height (feet → back) → 0..1 → colour ramp
        begin(2)
        sep = N("ShaderNodeSeparateXYZ")
        L(coords, sep.inputs[0])
        along = N("ShaderNodeMapRange")
        L(sep.outputs[axis], along.inputs["Value"])
        along.inputs["From Min"].default_value, along.inputs["From Max"].default_value = span
        col = color_ramp(along.outputs["Result"], ramp)
        end()

    if steps >= 3:  # 3 · Blotches: soft noise → tight ramp = soft-edged blobs → mix a second colour
        begin(3)
        noise = N("ShaderNodeTexNoise")
        noise.inputs["Scale"].default_value = 1.6
        noise.inputs["Detail"].default_value = 3.0
        noise.inputs["Distortion"].default_value = 0.8
        L(coords, noise.inputs["Vector"])
        blobs = color_ramp(noise.outputs["Fac"], [(0.42, (0, 0, 0)), (0.62, (1, 1, 1))])
        col = mix(scaled(blobs, blotch_amount), col, blotch)
        end()

    if steps >= 4 and stripe:  # 4 · Stripes: wobbly wave bands, only on faces that look up
        begin(4)
        wave = N("ShaderNodeTexWave")
        wave.bands_direction = "Y"
        wave.inputs["Scale"].default_value = stripe[1]
        wave.inputs["Distortion"].default_value = 6.0
        wave.inputs["Detail Scale"].default_value = 0.5
        L(coords, wave.inputs["Vector"])
        bands = color_ramp(wave.outputs["Fac"], [(0.55, (0, 0, 0)), (0.7, (1, 1, 1))])
        normal = N("ShaderNodeSeparateXYZ")
        L(N("ShaderNodeNewGeometry").outputs["Normal"], normal.inputs[0])
        up = N("ShaderNodeMapRange")
        L(normal.outputs["Z"], up.inputs["Value"])
        up.inputs["From Min"].default_value, up.inputs["From Max"].default_value = 0.2, 0.7
        mask = N("ShaderNodeMath")
        mask.operation = "MULTIPLY"
        L(bands, mask.inputs[0])
        L(up.outputs["Result"], mask.inputs[1])
        col = mix(scaled(mask.outputs[0], 0.6), col, stripe[0])
        end()

    if steps >= 5:  # 5 · Facet tint: one random number per face (Facets modifier) → hue + brightness
        begin(5)
        facet = N("ShaderNodeAttribute")
        facet.attribute_type, facet.attribute_name = "GEOMETRY", "facet"
        hue = N("ShaderNodeMapRange")
        L(facet.outputs["Fac"], hue.inputs["Value"])
        hue.inputs["To Min"].default_value, hue.inputs["To Max"].default_value = 0.47, 0.53
        val = N("ShaderNodeMapRange")
        L(facet.outputs["Fac"], val.inputs["Value"])
        val.inputs["To Min"].default_value, val.inputs["To Max"].default_value = 0.8, 1.15
        hsv = N("ShaderNodeHueSaturation")
        L(hue.outputs["Result"], hsv.inputs["Hue"])
        L(val.outputs["Result"], hsv.inputs["Value"])
        L(col, hsv.inputs["Color"])
        col = hsv.outputs["Color"]
        end()

    if steps >= 6:  # 6 · Grain: very fine faint noise multiplied in = paper fibres
        begin(6)
        grain = N("ShaderNodeTexNoise")
        grain.inputs["Scale"].default_value = 45.0
        grain.inputs["Detail"].default_value = 6.0
        L(coords, grain.inputs["Vector"])
        fibres = color_ramp(grain.outputs["Fac"], [(0.3, (0.9, 0.9, 0.9)), (0.7, (1.03, 1.03, 1.03))])
        col = mix(1.0, col, fibres, "MULTIPLY")
        end()

    if steps >= 7:  # 7 · Fold lines: Wireframe = 1 near every triangle edge → mix a pale cream
        begin(7)
        wire = N("ShaderNodeWireframe")
        wire.use_pixel_size = False                         # size in metres: lines scale with the model
        wire.inputs["Size"].default_value = line_size
        col = mix(scaled(wire.outputs["Fac"], 0.6), col, line)
        end()

    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Roughness"].default_value = 0.8            # matte, like paper
    bsdf.inputs["Specular IOR Level"].default_value = 0.25
    L(col, bsdf.inputs["Base Color"])
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    L(bsdf.outputs[0], out.inputs["Surface"])
    bsdf.location, out.location = (300, 0), (600, 0)
    return mat


def flat_material(name, rgb, emission=0.0, roughness=0.4):
    mat = bpy.data.materials.new(name)
    bsdf = mat.node_tree.nodes["Principled BSDF"]
    bsdf.inputs["Base Color"].default_value = (*rgb, 1)
    bsdf.inputs["Roughness"].default_value = roughness
    if emission:
        bsdf.inputs["Emission Color"].default_value = (*rgb, 1)
        bsdf.inputs["Emission Strength"].default_value = emission
    return mat


# ---------------------------------------------------------------- low-poly building blocks

def _finish(o, mat):
    o.data.materials.append(mat)
    return o


def blob(loc, scale, mat, subdiv=2, jitter=0.04, keep=0.55):
    """1. A lumpy low-poly piece: icosphere → scale → jitter verts → decimate to uneven triangles."""
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=subdiv, radius=1.0, location=loc)
    o = bpy.context.active_object
    o.scale = scale
    bpy.ops.object.transform_apply(scale=True)
    for v in o.data.vertices:
        v.co += Vector([random.uniform(-jitter, jitter) for _ in range(3)])
    if keep < 1.0:
        m = o.modifiers.new("dec", "DECIMATE")
        m.ratio = keep
        bpy.ops.object.modifier_apply(modifier="dec")
    return _finish(o, mat)


def spike(base, direction, radius, length, mat, sides=4):
    d = Vector(direction).normalized()
    bpy.ops.mesh.primitive_cone_add(vertices=sides, radius1=radius, radius2=0.0, depth=length,
                                    location=Vector(base) + d * (length * 0.5))
    o = bpy.context.active_object
    o.rotation_mode = "QUATERNION"
    o.rotation_quaternion = d.to_track_quat("Z", "Y")
    bpy.ops.object.transform_apply(rotation=True)
    return _finish(o, mat)


def join(objs, name):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    o = bpy.context.active_object
    o.name = o.data.name = name
    for p in o.data.polygons:
        p.use_smooth = False                                               # flat shaded facets
    tri = o.modifiers.new("tri", "TRIANGULATE")                            # fold lines on triangles
    bpy.ops.object.modifier_apply(modifier=tri.name)
    gn = o.modifiers.new("Facets", "NODES")
    gn.node_group = bpy.data.node_groups["Facets"]
    return o


def eye(center, direction, size, iris_rgb, parent):
    """5. Cartoon eye from stacked flat discs facing `direction`."""
    d = Vector(direction).normalized()
    right = d.cross(Vector((0, 0, 1))).normalized()
    up = right.cross(d).normalized()
    layers = [  # (radius, offset along d, in-plane offset (right, up), material)
        (1.0, 0.0, (0, 0), "EyeOutline"),
        (0.86, 0.012, (0, 0), "EyeWhite"),
        (0.52, 0.024, (0.06, -0.04), "Iris_" + parent.name),
        (0.3, 0.036, (0.08, -0.06), "Pupil"),
        (0.13, 0.048, (-0.12, 0.2), "EyeShine"),
        (0.07, 0.048, (0.22, -0.28), "EyeShine"),
    ]
    if ("Iris_" + parent.name) not in bpy.data.materials:
        flat_material("Iris_" + parent.name, iris_rgb, roughness=0.3)
    for r, off, (ox, oy), mat_name in layers:
        loc = Vector(center) + d * off + (right * ox + up * oy) * size
        bpy.ops.mesh.primitive_cylinder_add(vertices=40, radius=r * size, depth=0.006, location=loc)
        o = bpy.context.active_object
        o.rotation_mode = "QUATERNION"
        o.rotation_quaternion = d.to_track_quat("Z", "Y")
        o.data.materials.append(bpy.data.materials[mat_name])
        for p in o.data.polygons:
            p.use_smooth = True
        o.parent = parent
        o.matrix_parent_inverse = parent.matrix_world.inverted()


# ---------------------------------------------------------------- the two test subjects

FROG_SKIN = dict(
    ramp=[(0.0, (0.95, 0.62, 0.04)), (0.4, (0.3, 0.62, 0.03)), (1.0, (0.05, 0.3, 0.06))],
    axis="Z", span=(0.1, 1.7), blotch=(0.02, 0.18, 0.06), blotch_amount=0.6,
    stripe=((0.02, 0.14, 0.05), 2.2))


def frog():
    skin = paper_material("PaperFrog", **FROG_SKIN)
    feet = paper_material("PaperFrogFeet", ramp=[(0.0, (0.9, 0.22, 0.08)), (1.0, (0.95, 0.45, 0.1))],
                          axis="Z", span=(0.0, 0.5), blotch=(0.7, 0.08, 0.12))
    spikes = paper_material("PaperSpikes", ramp=[(0.0, (0.25, 0.08, 0.7)), (1.0, (0.7, 0.15, 0.6))],
                            axis="Z", span=(1.2, 2.0), blotch=(0.9, 0.2, 0.35))
    mouth = flat_material("Mouth", (0.25, 0.04, 0.06), roughness=0.7)
    tooth = paper_material("PaperTooth", ramp=[(0.0, (0.95, 0.92, 0.85)), (1.0, (1, 1, 1))],
                           axis="Z", span=(0.8, 1.1), blotch=(0.85, 0.8, 0.75), blotch_amount=0.2)
    parts = [
        blob((0, 0.15, 0.75), (1.2, 1.05, 0.78), skin),                    # body
        blob((0, -0.5, 1.05), (1.0, 0.82, 0.62), skin),                    # head
        blob((0.46, -0.55, 1.5), (0.4, 0.38, 0.38), skin, subdiv=1, keep=1.0),   # eye bumps
        blob((-0.46, -0.55, 1.5), (0.4, 0.38, 0.38), skin, subdiv=1, keep=1.0),
        blob((0.95, 0.45, 0.45), (0.42, 0.62, 0.42), skin, subdiv=1, keep=1.0),  # thighs
        blob((-0.95, 0.45, 0.45), (0.42, 0.62, 0.42), skin, subdiv=1, keep=1.0),
        blob((0.62, -0.72, 0.32), (0.2, 0.2, 0.34), skin, subdiv=1, keep=1.0),   # front legs
        blob((-0.62, -0.72, 0.32), (0.2, 0.2, 0.34), skin, subdiv=1, keep=1.0),
        blob((0.0, -1.12, 0.9), (0.62, 0.2, 0.14), mouth, subdiv=1, keep=1.0),   # open mouth
    ]
    for side in (1, -1):                                                   # feet + toes
        for fx, fy, s in ((0.7, -0.95, 0.28), (1.15, -0.1, 0.34)):
            parts.append(blob((side * fx, fy, 0.06), (s, s * 1.2, 0.08), feet, subdiv=1, keep=1.0))
            for k in (-1, 0, 1):
                ang = math.radians(-90 + k * 35 + side * 10)
                d = Vector((math.cos(ang) * side * 0.4 + k * 0.5 * side, math.sin(ang), 0.05))
                parts.append(spike((side * fx + k * 0.12, fy - s * 0.9, 0.05), d, 0.07, 0.2, feet))
    bpy.context.view_layer.update()
    depsgraph = bpy.context.evaluated_depsgraph_get()
    for i in range(7):                                                     # back spikes, ON the back:
        t = i / 6.0                                                        # shoot a ray down, put the
        y = -0.25 + t * 1.35                                               # spike where it hits
        hit, where, normal, *_ = bpy.context.scene.ray_cast(depsgraph, Vector((0, y, 4.0)), Vector((0, 0, -1)))
        if hit:
            d = (normal + Vector((0, 0.5, 0.6))).normalized()
            parts.append(spike(where - d * 0.04, d, 0.14 - t * 0.06, 0.42 - t * 0.22, spikes))
    for k in range(5):                                                     # teeth on the upper lip
        x = -0.4 + k * 0.2
        parts.append(spike((x, -1.26, 1.0), (0, -0.25, -1.0), 0.05, 0.13, tooth, sides=3))
    body = join(parts, "Frog")
    for side in (1, -1):
        eye((side * 0.55, -0.95, 1.6), (side * 0.3, -1.0, 0.25), 0.27, (0.25, 0.75, 0.85), body)
    return body


def step_row(frog_obj):
    """7 linked copies of the frog (same mesh, same eyes) in a row behind the scene, each with
    one step material Paper_<k>_<Name> on its skin, plus a label. Camera "StepsCamera" sees them."""
    labels = flat_material("Label", (1, 1, 1), emission=1.0)
    skin_slot = [m.name for m in frog_obj.data.materials].index("PaperFrog")
    col = bpy.data.collections.new("Steps")
    bpy.context.scene.collection.children.link(col)
    for k in range(1, 8):
        mat = paper_material(f"Paper_{k}_{STEP_NAMES[k - 1]}", **FROG_SKIN, steps=k)
        x = (k - 4) * 3.2
        copy = frog_obj.copy()                       # shares the mesh: remodel once, all 7 follow
        copy.name = f"Frog_step{k}"
        copy.location = (x, 12.0, 0.0)
        col.objects.link(copy)
        copy.material_slots[skin_slot].link = "OBJECT"   # this copy's own material on the skin slot
        copy.material_slots[skin_slot].material = mat
        for child in frog_obj.children:              # the eye discs
            c = child.copy()
            c.parent = copy
            col.objects.link(c)
        bpy.ops.object.text_add(location=(x - 1.1, 13.2, 2.3))
        text = bpy.context.active_object
        text.name = f"Label_step{k}"
        text.data.body = f"{k} {STEP_NAMES[k - 1]}"
        text.data.size = 0.42
        text.rotation_euler = (math.radians(60), 0, 0)
        text.data.materials.append(labels)
        for c in list(text.users_collection):
            c.objects.unlink(text)
        col.objects.link(text)
    cam = bpy.data.objects.new("StepsCamera", bpy.data.cameras.new("StepsCamera"))
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 23.0
    cam.location = (0.0, 5.5, 6.0)                    # in front of the row, behind the main frog
    cam.rotation_euler = (math.radians(60), 0, 0)     # looking down a bit: backs + stripes show
    bpy.context.scene.collection.objects.link(cam)


def rock():
    stone = paper_material(
        "PaperRock",
        ramp=[(0.0, (0.14, 0.16, 0.3)), (0.55, (0.32, 0.33, 0.45)), (1.0, (0.2, 0.4, 0.08))],
        axis="Z", span=(0.0, 1.1), blotch=(0.55, 0.3, 0.2), blotch_amount=0.45)
    r = join([blob((2.5, -1.7, 0.45), (0.95, 0.8, 0.62), stone, jitter=0.12, keep=0.6),
              blob((3.3, -1.1, 0.25), (0.45, 0.4, 0.35), stone, subdiv=1, jitter=0.06, keep=1.0)], "Rock")
    return r


# ---------------------------------------------------------------- scene

def scene():
    sc = bpy.context.scene
    world = bpy.data.worlds.new("World")
    sc.world = world
    bg = world.node_tree.nodes["Background"]
    bg.inputs["Color"].default_value = (0.32, 0.42, 0.58, 1)
    bg.inputs["Strength"].default_value = 0.35
    bpy.ops.mesh.primitive_plane_add(size=40, location=(0, 0, 0))
    ground = bpy.context.active_object
    ground.name = "Ground"
    ground.data.materials.append(flat_material("Ground", (0.1, 0.16, 0.3), roughness=1.0))
    sun = bpy.data.objects.new("Sun", bpy.data.lights.new("Sun", "SUN"))
    sun.data.energy = 4.5
    sun.data.angle = math.radians(12)                                      # soft shadow edges
    sun.rotation_euler = (math.radians(50), math.radians(10), math.radians(-35))
    sc.collection.objects.link(sun)
    target = bpy.data.objects.new("CamTarget", None)
    target.location = (1.0, -0.6, 0.8)
    sc.collection.objects.link(target)
    cam = bpy.data.objects.new("Camera", bpy.data.cameras.new("Camera"))
    cam.data.lens = 50
    cam.location = (-5.2, -6.4, 3.4)
    con = cam.constraints.new("TRACK_TO")
    con.target = target
    sc.collection.objects.link(cam)
    sc.camera = cam
    sc.render.engine = "BLENDER_EEVEE"
    sc.view_settings.view_transform = "Standard"
    sc.render.resolution_x, sc.render.resolution_y = 1400, 1000


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    facet_modifier_group()
    flat_material("EyeOutline", (0.03, 0.03, 0.05), roughness=0.5)
    flat_material("EyeWhite", (0.97, 0.97, 0.97), roughness=0.25)
    flat_material("Pupil", (0.02, 0.02, 0.03), roughness=0.2)
    flat_material("EyeShine", (1, 1, 1), emission=2.0)
    step_row(frog())
    rock()
    scene()
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, "papercraft.blend"))
    sc = bpy.context.scene
    sc.render.filepath = os.path.join(HERE, "preview.png")
    bpy.ops.render.render(write_still=True)
    main_cam, sc.camera = sc.camera, bpy.data.objects["StepsCamera"]   # the step row
    sc.render.resolution_x, sc.render.resolution_y = 2400, 700
    sc.render.filepath = os.path.join(HERE, "steps_preview.png")
    bpy.ops.render.render(write_still=True)
    print("papercraft done")


if __name__ == "__main__":
    main()
