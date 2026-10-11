"""Build clay.blend: a plasticine critter; every part's material is the same clay recipe.

Run:  Blender -b --python make_clay.py      (saves clay.blend, preview.png, steps_preview.png)

Collection "Steps", 30 m to the right of the hero (camera StepsCamera): the critter 7 times, each using
materials with only steps 1..k (ClayTeal_1 … ClayTeal_7): 1 Base · 2 Subsurface · 3 Mottle ·
4 Specks · 5 Dents · 6 Fingerprints · 7 Grain (= the hero's ClayTeal).

What makes clay read as clay:
  SHAPE     1. soft rounded pieces, slightly lumpy (subdivision + a gentle Displace modifier)
            2. separate pieces pressed together: ball eyes, bead pupils, rolled mouth, flat cheeks
  MATERIAL  3. subsurface scattering: light sinks in a little → soft warm shadows, glowing edges
            4. soft sheen: roughness ~0.5 (not matte paper, not shiny plastic)
            5. hand marks as bump only: fingerprint whorls + shallow thumb dents + fine grain
            6. mottled colour (kneaded clay is never one flat colour) + tiny dust specks
  STAGING   7. big soft lights, warm key / cool rim, a little depth of field (miniature look)

Each material is plain nodes (no node group), left → right into the Principled BSDF, one frame
per step. Settings are labelled Value nodes inside their step: Roughness, Subsurface, Mottle,
Specks, Dents, Bump Strength, Fingerprints; the colour is the "Clay colour" RGB node in step 1.
"""
import math
import os
import random

import bpy
from mathutils import Vector

HERE = os.path.dirname(os.path.abspath(__file__))
random.seed(3)


# ---------------------------------------------------------------- the clay node group

STEP_NAMES = ["Base", "Subsurface", "Mottle", "Specks", "Dents", "Fingerprints", "Grain"]
DEFAULTS = {"Roughness": 0.5, "Subsurface": 0.35, "Mottle": 0.45, "Specks": 0.5,
            "Dents": 1.0, "Bump Strength": 0.7, "Fingerprints": 1.0}
MATERIAL_PARAMS = {}   # material name → (rgb, overrides): lets the step row rebuild each part per step


def clay_material(name, rgb, steps=7, **overrides):
    """The clay look as ONE plain material (no node group), built up in 7 steps; `steps` stops
    after that many. Everything flows left → right into the Principled BSDF on the right.
    Each step's nodes sit in a frame labelled with the step; its settings are labelled Value
    nodes inside that frame (Mottle, Dents, …), so every node is wired to something."""
    params = {**DEFAULTS, **{k.replace("_", " "): v for k, v in overrides.items()}}
    mat = bpy.data.materials.new(name)
    nt = mat.node_tree
    nt.nodes.clear()
    L = nt.links.new
    stage = {"k": 0, "nodes": [], "x": -2600}

    def N(kind):
        n = nt.nodes.new(kind)
        stage["nodes"].append(n)
        return n

    def begin(k):
        stage["k"], stage["nodes"] = k, []

    def end():
        """Frame this step's nodes, in columns of 8, then move the x cursor past them."""
        k = stage["k"]
        frame = nt.nodes.new("NodeFrame")
        frame.label = f"{k} · {STEP_NAMES[k - 1]}"
        frame.label_size = 20
        for i, n in enumerate(stage["nodes"]):
            n.location = (stage["x"] + (i // 8) * 230, 300 - (i % 8) * 190)
            n.parent = frame
        stage["x"] += 230 * ((len(stage["nodes"]) - 1) // 8 + 1) + 120

    def setting(key):
        """A labelled Value node holding one of this material's settings."""
        v = N("ShaderNodeValue")
        v.label = key
        v.outputs[0].default_value = params[key]
        return v.outputs[0]

    def math_node(op, a, b=None):
        n = N("ShaderNodeMath")
        n.operation = op
        for s, v in zip(n.inputs, (a, b)):
            if v is None:
                continue
            if isinstance(v, bpy.types.NodeSocket):
                L(v, s)
            else:
                s.default_value = v
        return n.outputs[0]

    def ramp(fac, stops):
        r = N("ShaderNodeValToRGB")
        for el, (pos, val) in zip(r.color_ramp.elements, stops):
            el.position, el.color = pos, (val, val, val, 1)
        L(fac, r.inputs["Fac"])
        return r.outputs["Color"]

    def mix(fac, a, b):
        m = N("ShaderNodeMix")
        m.data_type = "RGBA"
        for s, v in ((m.inputs[0], fac), (m.inputs[6], a), (m.inputs[7], b)):
            if isinstance(v, bpy.types.NodeSocket):
                L(v, s)
            else:
                s.default_value = (*v, 1)
        return m.outputs[2]

    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    L(bsdf.outputs[0], out.inputs["Surface"])
    bsdf.inputs["Specular IOR Level"].default_value = 0.45

    # 1 · Base: the clay colour, roughness 0.5. On soft lumpy shapes: already a plastic toy.
    begin(1)
    base = N("ShaderNodeRGB")
    base.label = "Clay colour"
    base.outputs[0].default_value = (*rgb, 1)
    col = base.outputs[0]
    L(setting("Roughness"), bsdf.inputs["Roughness"])
    end()

    if steps >= 2:  # 2 · Subsurface: light sinks in → soft warm shadows (radius/coat set on the BSDF)
        begin(2)
        L(setting("Subsurface"), bsdf.inputs["Subsurface Weight"])
        bsdf.inputs["Subsurface Radius"].default_value = (0.9, 0.55, 0.4)
        bsdf.inputs["Subsurface Scale"].default_value = 0.05
        bsdf.inputs["Coat Weight"].default_value = 0.08          # faint oily layer
        bsdf.inputs["Coat Roughness"].default_value = 0.35
        end()

    co = None
    if steps >= 3:  # 3 · Mottle: soft noise picks between the colour and a darker, warmer copy
        begin(3)
        co = N("ShaderNodeTexCoord").outputs["Object"]           # each piece carries its own pattern
        darker = N("ShaderNodeHueSaturation")
        darker.inputs["Hue"].default_value = 0.52
        darker.inputs["Value"].default_value = 0.78
        L(col, darker.inputs["Color"])
        noise = N("ShaderNodeTexNoise")
        noise.inputs["Scale"].default_value = 4.0
        noise.inputs["Detail"].default_value = 3.0
        L(co, noise.inputs["Vector"])
        amount = math_node("MULTIPLY", ramp(noise.outputs["Fac"], [(0.4, 0.0), (0.7, 1.0)]), setting("Mottle"))
        col = mix(amount, col, darker.outputs["Color"])
        end()

    if steps >= 4:  # 4 · Specks: tiny Voronoi cells, a dark dot near the centre of 1 cell in 4
        begin(4)
        vor = N("ShaderNodeTexVoronoi")
        vor.inputs["Scale"].default_value = 90.0
        L(co, vor.inputs["Vector"])
        near = math_node("LESS_THAN", vor.outputs["Distance"], 0.08)
        cell = N("ShaderNodeSeparateColor")
        L(vor.outputs["Color"], cell.inputs[0])
        picked = math_node("LESS_THAN", cell.outputs["Red"], 0.25)
        amount = math_node("MULTIPLY", math_node("MULTIPLY", near, picked), setting("Specks"))
        col = mix(amount, col, (0.05, 0.04, 0.04))
        end()

    height, bump = None, None
    if steps >= 5:  # 5 · Dents: smooth Voronoi distance = round shallow thumb cups → Bump → Normal
        begin(5)
        dvor = N("ShaderNodeTexVoronoi")
        dvor.feature = "SMOOTH_F1"
        dvor.inputs["Scale"].default_value = 5.0
        L(co, dvor.inputs["Vector"])
        height = math_node("MULTIPLY", math_node("POWER", dvor.outputs["Distance"], 0.6), setting("Dents"))
        bump = N("ShaderNodeBump")
        bump.inputs["Distance"].default_value = 0.02
        L(setting("Bump Strength"), bump.inputs["Strength"])
        L(bump.outputs["Normal"], bsdf.inputs["Normal"])
        end()

    if steps >= 6:  # 6 · Fingerprints: rings around the centre of a few big Voronoi cells, added to the height
        begin(6)
        fvor = N("ShaderNodeTexVoronoi")
        fvor.inputs["Scale"].default_value = 3.0
        L(co, fvor.inputs["Vector"])
        offset = N("ShaderNodeVectorMath")
        offset.operation = "SUBTRACT"
        L(co, offset.inputs[0])
        L(fvor.outputs["Position"], offset.inputs[1])
        dist = N("ShaderNodeVectorMath")
        dist.operation = "LENGTH"
        L(offset.outputs[0], dist.inputs[0])
        wobble = N("ShaderNodeTexNoise")                         # whorls are never perfect circles
        wobble.inputs["Scale"].default_value = 12.0
        L(co, wobble.inputs["Vector"])
        phase = math_node("ADD", math_node("MULTIPLY", dist.outputs["Value"], 60.0),
                          math_node("MULTIPLY", wobble.outputs["Fac"], 4.0))
        ridges = math_node("MULTIPLY", math_node("SINE", phase), 0.5)
        fcell = N("ShaderNodeSeparateColor")
        L(fvor.outputs["Color"], fcell.inputs[0])
        touched = math_node("LESS_THAN", fcell.outputs["Red"], 0.4)      # 4 cells in 10
        patch = N("ShaderNodeMapRange")                                  # fade out at the patch edge
        L(fvor.outputs["Distance"], patch.inputs["Value"])
        patch.inputs["From Min"].default_value, patch.inputs["From Max"].default_value = 0.35, 0.15
        fp = math_node("MULTIPLY", math_node("MULTIPLY", ridges, touched), patch.outputs["Result"])
        fp = math_node("MULTIPLY", math_node("MULTIPLY", fp, setting("Fingerprints")), 0.3)
        height = math_node("ADD", height, fp)
        end()

    if steps >= 7:  # 7 · Grain: very fine noise added to the height + a little roughness variation
        begin(7)
        grain = N("ShaderNodeTexNoise")
        grain.inputs["Scale"].default_value = 160.0
        grain.inputs["Detail"].default_value = 4.0
        L(co, grain.inputs["Vector"])
        height = math_node("ADD", height, math_node("MULTIPLY", grain.outputs["Fac"], 0.06))
        wiggle = math_node("MULTIPLY", math_node("SUBTRACT", grain.outputs["Fac"], 0.5), 0.2)
        L(math_node("ADD", bsdf.inputs["Roughness"].links[0].from_socket, wiggle), bsdf.inputs["Roughness"])
        end()

    if bump is not None:
        L(height, bump.inputs["Height"])
    L(col, bsdf.inputs["Base Color"])
    bsdf.location = (stage["x"] + 60, 300)
    out.location = (stage["x"] + 400, 300)
    return mat


def clay(name, rgb, **overrides):
    """A part's final material (all 7 steps). Remembered so the step row can rebuild it per step."""
    MATERIAL_PARAMS[name] = (rgb, overrides)
    return clay_material(name, rgb, 7, **overrides)


STEPS_X = 30.0   # the step row sits 30 m to the right of the hero critter (same scene)


def step_row(critter_objects):
    """Collection "Steps" in the main scene, 30 m to the right: 7 linked copies of the critter,
    each part using that part's material rebuilt with only steps 1..k (e.g. ClayTeal_4). Own lights, labels and ortho camera "StepsCamera" (select it + Numpad 0 to look)."""
    sc = bpy.context.scene
    col = bpy.data.collections.new("Steps")
    sc.collection.children.link(col)
    label_mat = bpy.data.materials.new("Label")
    label_mat.node_tree.nodes["Principled BSDF"].inputs["Emission Color"].default_value = (1, 1, 1, 1)
    label_mat.node_tree.nodes["Principled BSDF"].inputs["Emission Strength"].default_value = 1.0
    for k in range(1, 8):
        x = STEPS_X + (k - 4) * 2.7
        for o in critter_objects:
            c = o.copy()                                   # shares the mesh: reshape once, all follow
            c.name = f"{o.name}_step{k}"
            c.location = o.location + Vector((x, 0, 0))
            col.objects.link(c)
            for slot in c.material_slots:
                base = slot.material.name
                rgb, inputs = MATERIAL_PARAMS[base]
                step_name = f"{base}_{k}"
                mat = bpy.data.materials.get(step_name) or clay_material(step_name, rgb, k, **inputs)
                slot.link = "OBJECT"                       # this copy's own material on that slot
                slot.material = mat
        cu = bpy.data.curves.new(f"Label{k}", "FONT")
        cu.body = f"{k} {STEP_NAMES[k - 1]}"
        cu.size = 0.36
        cu.align_x = "CENTER"
        cu.materials.append(label_mat)
        t = bpy.data.objects.new(f"Label_step{k}", cu)
        t.location = (x, 0.6, 2.25)
        t.rotation_euler = (math.radians(70), 0, 0)
        col.objects.link(t)
    key = bpy.data.objects.new("StepsKey", bpy.data.lights.new("StepsKey", "AREA"))
    key.data.energy, key.data.shape, key.data.size, key.data.size_y = 4500, "RECTANGLE", 22.0, 4.0
    key.location = (STEPS_X - 2.0, -5.0, 6.0)
    key.rotation_euler = (math.radians(42), 0, math.radians(-12))
    col.objects.link(key)
    rim = bpy.data.objects.new("StepsRim", bpy.data.lights.new("StepsRim", "AREA"))
    rim.data.energy, rim.data.size, rim.data.color = 2000, 20.0, (0.75, 0.85, 1.0)
    rim.location = (STEPS_X, 5.0, 4.0)
    rim.rotation_euler = (math.radians(-45), 0, 0)
    col.objects.link(rim)
    cam = bpy.data.objects.new("StepsCamera", bpy.data.cameras.new("StepsCamera"))
    cam.data.type, cam.data.ortho_scale = "ORTHO", 19.5
    cam.location = (STEPS_X, -8.0, 4.6)
    cam.rotation_euler = (math.radians(70), 0, 0)
    col.objects.link(cam)
    return cam


# ---------------------------------------------------------------- clay pieces

LUMPS = []


def piece(loc, scale, mat, rot=(0, 0, 0), lumpy=0.03, kind="sphere"):
    """1. One soft piece: smooth ball (or cone) + gentle lumps + subdivision, shaded smooth."""
    if not LUMPS:
        tex = bpy.data.textures.new("Lumps", "CLOUDS")
        tex.noise_scale = 0.35
        LUMPS.append(tex)
    if kind == "cone":
        bpy.ops.mesh.primitive_cone_add(vertices=24, radius1=1.0, radius2=0.12, depth=2.0, location=loc)
    else:
        bpy.ops.mesh.primitive_uv_sphere_add(segments=32, ring_count=16, radius=1.0, location=loc)
    o = bpy.context.active_object
    o.scale = scale
    o.rotation_euler = [r + random.uniform(-0.05, 0.05) for r in rot]      # nothing hand-made is exact
    o.data.materials.append(mat)
    if lumpy:
        d = o.modifiers.new("Lumps", "DISPLACE")
        d.texture, d.strength, d.texture_coords = LUMPS[0], lumpy, "GLOBAL"
    s = o.modifiers.new("Smooth", "SUBSURF")
    s.levels = s.render_levels = 2
    bpy.ops.object.shade_smooth()
    return o


def smile(center, width, thickness, mat):
    """2. A rolled clay snake bent into a smile, facing −Y: a curved tube (curve + round bevel)
    with a little ball on each end so the ends are rounded like a real rolled snake."""
    cu = bpy.data.curves.new("Smile", "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth, cu.bevel_resolution, cu.use_fill_caps = thickness, 4, True
    sp = cu.splines.new("POLY")
    n = 13
    sp.points.add(n - 1)
    ends = []
    for k in range(n):
        a = math.radians(-150 + 120 * k / (n - 1))           # arc from 210° to 330°: a smile
        p = Vector((math.cos(a) * width, 0.0, math.sin(a) * width * 0.75 + width * 0.6))
        sp.points[k].co = (*p, 1.0)
        if k in (0, n - 1):
            ends.append(p)
    o = bpy.data.objects.new("Smile", cu)
    o.location = center
    bpy.context.scene.collection.objects.link(o)
    cu.materials.append(mat)
    for p in ends:
        piece(Vector(center) + p, (thickness,) * 3, mat, lumpy=0.0)
    return o


def critter():
    """Returns every object of the critter (for the step scene)."""
    before = set(bpy.data.objects)
    teal = clay("ClayTeal", (0.01, 0.22, 0.3))
    belly = clay("ClayBelly", (0.95, 0.3, 0.03))
    horn = clay("ClayHorn", (0.95, 0.55, 0.01), Fingerprints=0.6)
    cheek = clay("ClayCheek", (0.9, 0.22, 0.32), Fingerprints=0.0, Dents=0.3)
    white = clay("ClayWhite", (0.85, 0.84, 0.8), Mottle=0.1, Fingerprints=0.4, Subsurface=0.5)
    black = clay("ClayBlack", (0.012, 0.012, 0.015), Mottle=0.0, Specks=0.0, Roughness=0.35)
    piece((0, 0, 0.85), (0.9, 0.8, 0.85), teal, lumpy=0.08)                         # body
    piece((0, -0.6, 0.64), (0.46, 0.18, 0.4), belly, rot=(math.radians(-18), 0, 0))  # belly patch
    for side in (1, -1):
        piece((side * 0.38, -0.22, 0.1), (0.28, 0.36, 0.13), teal)                  # feet
        piece((side * 0.84, -0.12, 0.72), (0.14, 0.14, 0.32), teal,                  # arms
              rot=(0, math.radians(side * 30), 0))
        piece((side * 0.36, 0.05, 1.68), (0.12, 0.12, 0.2), horn, kind="cone",       # horns
              rot=(0, math.radians(side * 22), 0), lumpy=0.0)
        eye_c = Vector((side * 0.3, -0.62, 1.16))
        piece(eye_c, (0.24, 0.22, 0.24), white, lumpy=0.0)                           # eyeball
        look = Vector((side * 0.12 - 0.05, -1.0, -0.05)).normalized()
        piece(eye_c + look * 0.2, (0.11, 0.05, 0.12), black, lumpy=0.0,               # pupil bead
              rot=(0, 0, -math.atan2(look.x, -look.y)))
        piece(eye_c + look * 0.25 + Vector((-0.04, 0, 0.05)), (0.03, 0.02, 0.03), white, lumpy=0.0)
        piece((side * 0.55, -0.6, 0.92), (0.11, 0.04, 0.08), cheek, lumpy=0.0,       # cheeks
              rot=(0, 0, math.radians(side * 32)))
    smile((0, -0.8, 0.92), 0.17, 0.045, black)
    return [o for o in bpy.data.objects if o not in before]


# ---------------------------------------------------------------- staging

def stage():
    sc = bpy.context.scene
    world = bpy.data.worlds.new("World")
    sc.world = world
    world.node_tree.nodes["Background"].inputs["Color"].default_value = (0.5, 0.45, 0.42, 1)
    world.node_tree.nodes["Background"].inputs["Strength"].default_value = 0.25
    bpy.ops.mesh.primitive_plane_add(size=120, location=(15, 0, 0))
    table = bpy.context.active_object
    table.name = "Table"
    table.data.materials.append(clay("ClayTable", (0.55, 0.45, 0.36), Fingerprints=0.0, Specks=0.2))
    target = bpy.data.objects.new("Focus", None)
    target.location = (0, -0.2, 0.85)
    sc.collection.objects.link(target)

    def area(name, loc, energy, size, rgb):
        light = bpy.data.objects.new(name, bpy.data.lights.new(name, "AREA"))
        light.data.energy, light.data.size, light.data.color = energy, size, rgb
        light.location = loc
        c = light.constraints.new("TRACK_TO")
        c.target = target
        sc.collection.objects.link(light)

    area("Key", (-3.0, -3.5, 4.0), 900, 3.0, (1.0, 0.93, 0.85))                 # warm, big, soft
    area("Fill", (3.5, -2.5, 1.5), 250, 3.0, (0.85, 0.9, 1.0))
    area("Rim", (1.0, 3.5, 3.0), 600, 2.0, (0.75, 0.85, 1.0))                   # cool edge light
    cam = bpy.data.objects.new("Camera", bpy.data.cameras.new("Camera"))
    cam.data.lens = 70
    cam.data.dof.use_dof = True                                                 # 7. miniature look
    cam.data.dof.focus_object = target
    cam.data.dof.aperture_fstop = 0.5
    cam.location = (2.0, -6.2, 2.4)
    c = cam.constraints.new("TRACK_TO")
    c.target = target
    sc.collection.objects.link(cam)
    sc.camera = cam
    sc.render.engine = "BLENDER_EEVEE"
    sc.view_settings.view_transform = "AgX"
    for look in ("AgX - Punchy", "Punchy"):            # AgX alone washes clay colours out
        try:
            sc.view_settings.look = look
            break
        except TypeError:
            pass
    sc.render.resolution_x, sc.render.resolution_y = 1200, 1200


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    parts = critter()
    stage()
    steps_cam = step_row(parts)
    sc = bpy.context.scene
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(HERE, "clay.blend"))
    sc.render.filepath = os.path.join(HERE, "preview.png")
    bpy.ops.render.render(write_still=True)
    sc.camera = steps_cam                                   # render the step row too
    sc.render.resolution_x, sc.render.resolution_y = 2400, 800
    sc.render.filepath = os.path.join(HERE, "steps_preview.png")
    bpy.ops.render.render(write_still=True)
    print("clay done")


if __name__ == "__main__":
    main()
