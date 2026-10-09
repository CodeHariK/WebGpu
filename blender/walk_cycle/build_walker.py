"""Pip — a little character made of spheres and cylinders, on an armature, with an in-place walk cycle.

One rigid mesh per bone (no skinning or weights), the same setup the game's IK creatures use.
Facing −Y (Blender front), Z up, about 1.6 m tall. The walk: 24 frames (1 s at 24 fps), two steps,
looping (Cycles modifiers), in place (the root doesn't move: the game moves the character).

Keyed poses per step (the classic four): CONTACT (heel strikes, legs widest), DOWN (weight lands,
lowest), PASSING (swing leg passes the standing one), UP (push off, highest). The other leg and
the arms run half a cycle behind; hips bob, sway and twist; the spine and head counter-rotate.
"""
import bpy
import bmesh
import math
from mathutils import Matrix, Vector

FPS = 24
CYCLE = 24 # frames per cycle (two steps)

# Bones: name → (head, tail, parent). Character's left = +X.
BONES = {
    "root": ((0, 0, 0), (0, 0.3, 0), None),
    "hips": ((0, 0, 0.82), (0, 0, 0.98), "root"),
    "spine": ((0, 0, 0.98), (0, 0, 1.28), "hips"),
    "head": ((0, 0, 1.28), (0, 0, 1.55), "spine"),
}
for side, x in (("L", 1), ("R", -1)):
    BONES.update({
        f"thigh.{side}": ((0.12 * x, 0, 0.84), (0.12 * x, 0, 0.47), "hips"),
        f"shin.{side}": ((0.12 * x, 0, 0.47), (0.12 * x, 0, 0.11), f"thigh.{side}"),
        f"foot.{side}": ((0.12 * x, 0, 0.11), (0.12 * x, -0.17, 0.05), f"shin.{side}"),
        f"upperarm.{side}": ((0.25 * x, 0, 1.22), (0.29 * x, 0, 0.98), "spine"),
        f"forearm.{side}": ((0.29 * x, 0, 0.98), (0.31 * x, -0.02, 0.76), f"upperarm.{side}"),
    })

COLORS = {
    "skin": (1.0, 0.72, 0.55), "shirt": (0.1, 0.62, 0.62), "pants": (0.22, 0.2, 0.32),
    "shoe": (0.45, 0.22, 0.1), "glove": (0.95, 0.95, 0.92), "eye": (0.04, 0.04, 0.06), "hat": (0.95, 0.55, 0.1),
}


def material(name):
    key = f"M_{name}"
    if key in bpy.data.materials:
        return bpy.data.materials[key]
    mat = bpy.data.materials.new(key)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*COLORS[name], 1)
    bsdf.inputs["Roughness"].default_value = 0.6
    mat.diffuse_color = (*COLORS[name], 1)
    return mat


def make_armature():
    data = bpy.data.armatures.new("PipRig")
    arm = bpy.data.objects.new("Pip", data)
    bpy.context.scene.collection.objects.link(arm)
    data.display_type = "STICK"
    arm.show_in_front = True
    bpy.context.view_layer.objects.active = arm
    arm.select_set(True)
    window = bpy.context.window_manager.windows[0]
    with bpy.context.temp_override(window=window, active_object=arm, object=arm, selected_objects=[arm]):
        bpy.ops.object.mode_set(mode="EDIT")
        for name, (head, tail, parent) in BONES.items():
            bone = data.edit_bones.new(name)
            bone.head, bone.tail = head, tail
            bone.roll = 0.0
            if parent:
                bone.parent = data.edit_bones[parent]
                bone.use_connect = Vector(head) == data.edit_bones[parent].tail
        bpy.ops.object.mode_set(mode="OBJECT")
    for pb in arm.pose.bones:
        pb.rotation_mode = "XYZ"
    return arm


# --- the body: primitives, each parented to one bone --------------------------------------------

def _cylinder(bm, a, b, r_a, r_b, segments=12):
    a, b = Vector(a), Vector(b)
    span = b - a
    rot = Vector((0, 0, 1)).rotation_difference(span.normalized()).to_matrix().to_4x4()
    bmesh.ops.create_cone(bm, cap_ends=True, segments=segments, radius1=r_a, radius2=r_b, depth=span.length, matrix=Matrix.Translation((a + b) / 2) @ rot)


def _sphere(bm, at, radius, squash=(1, 1, 1)):
    m = Matrix.Translation(Vector(at)) @ Matrix.Diagonal((*squash, 1))
    bmesh.ops.create_uvsphere(bm, u_segments=16, v_segments=10, radius=radius, matrix=m)


def part(arm, name, bone, color, build):
    """A mesh built in rest position (world space), then parented to `bone` without moving."""
    bm = bmesh.new()
    build(bm)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    for poly in mesh.polygons:
        poly.use_smooth = True
    mesh.materials.append(material(color))
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    b = arm.data.bones[bone]
    tail = arm.matrix_world @ b.matrix_local @ Matrix.Translation((0, b.length, 0)) # bone parenting hangs children on the tail
    obj.parent = arm
    obj.parent_type = "BONE"
    obj.parent_bone = bone
    obj.matrix_parent_inverse = tail.inverted()
    return obj


def make_body(arm):
    part(arm, "Pants", "hips", "pants", lambda bm: (_cylinder(bm, (0, 0, 0.78), (0, 0, 0.98), 0.17, 0.18), _sphere(bm, (0, 0, 0.8), 0.17, (1, 0.9, 0.6))))
    part(arm, "Shirt", "spine", "shirt", lambda bm: (_cylinder(bm, (0, 0, 0.95), (0, 0, 1.24), 0.19, 0.17), _sphere(bm, (0, -0.02, 1.08), 0.21, (1, 0.95, 1)), _sphere(bm, (0, 0, 1.24), 0.17, (1.2, 0.9, 0.5))))
    part(arm, "Head", "head", "skin", lambda bm: _sphere(bm, (0, 0, 1.53), 0.27))
    part(arm, "Eyes", "head", "eye", lambda bm: [_sphere(bm, (x, -0.235, 1.57), 0.045, (0.8, 0.6, 1.5)) for x in (0.085, -0.085)])
    part(arm, "Hat", "head", "hat", lambda bm: (_cylinder(bm, (0, 0.02, 1.70), (0.03, 0.06, 2.0), 0.22, 0.02, 16), _cylinder(bm, (0, 0, 1.68), (0, 0, 1.72), 0.29, 0.28, 20)))
    for side, x in (("L", 1), ("R", -1)):
        part(arm, f"Thigh.{side}", f"thigh.{side}", "pants", lambda bm, x=x: (_cylinder(bm, (0.12 * x, 0, 0.84), (0.12 * x, 0, 0.47), 0.09, 0.075), _sphere(bm, (0.12 * x, 0, 0.47), 0.077)))
        part(arm, f"Shin.{side}", f"shin.{side}", "pants", lambda bm, x=x: _cylinder(bm, (0.12 * x, 0, 0.47), (0.12 * x, 0, 0.12), 0.072, 0.06))
        part(arm, f"Shoe.{side}", f"foot.{side}", "shoe", lambda bm, x=x: _sphere(bm, (0.12 * x, -0.06, 0.07), 0.1, (1.05, 1.7, 0.75)))
        part(arm, f"UpperArm.{side}", f"upperarm.{side}", "shirt", lambda bm, x=x: (_sphere(bm, (0.25 * x, 0, 1.22), 0.08), _cylinder(bm, (0.25 * x, 0, 1.22), (0.29 * x, 0, 0.98), 0.07, 0.06)))
        part(arm, f"Forearm.{side}", f"forearm.{side}", "skin", lambda bm, x=x: _cylinder(bm, (0.29 * x, 0, 0.98), (0.31 * x, -0.02, 0.78), 0.055, 0.05))
        part(arm, f"Glove.{side}", f"forearm.{side}", "glove", lambda bm, x=x: _sphere(bm, (0.31 * x, -0.02, 0.74), 0.085))


# --- the walk ----------------------------------------------------------------------------------
# Angles in degrees, in plain words; to_pose() turns them into bone rotations. (Measured on this rig:
# +X on a thigh or upper arm swings it back, +X on a shin bends the knee, +X on a foot points the
# toe down, +X on a forearm bends the elbow, +X on the spine leans forward. Hips: local Y is up (yaw),
# local Z is forward (roll: + lifts the left side).)

# One leg through the cycle: frame → (thigh forward, knee bend, toe up). The other leg is 12 frames on.
LEG = {
    1: (25, 3, 15),     # CONTACT: heel strikes, leg reaching forward, toe up
    4: (18, 22, 0),     # DOWN: weight lands, knee soaks it up, foot flat
    7: (2, 8, 0),       # PASSING: standing straight under the body
    10: (-14, 6, -15),  # UP: pushing the body up and on, heel lifting
    13: (-24, 12, -30), # the other foot's CONTACT: this one is behind, toe pushing off
    16: (-12, 55, -10), # lifts: knee bends hard, foot trails
    19: (15, 70, 5),    # PASSING (swing): knee high, foot tucked under
    22: (30, 25, 15),   # reaching forward again, toe up for the heel strike
}
# One arm: frame → (arm forward, elbow bend). Opposite to its leg: back while that leg is forward.
ARM = {1: (-20, 15), 7: (0, 20), 13: (22, 35), 19: (0, 20)}
# The body, per step (12 frames; the second step mirrors it): frame → (drop, hip yaw, hip roll)
BODY = {1: (0.0, -7, 0), 4: (-0.05, -4, 4), 7: (-0.01, 0, 5), 10: (0.03, 4, 2)}
SPINE_LEAN = 4
SHOULDER_COUNTER = 1.3 # the shoulders twist this × the hips, the other way


def _key(pb, frame, x=0.0, y=0.0, z=0.0):
    pb.rotation_euler = (math.radians(x), math.radians(y), math.radians(z))
    pb.keyframe_insert("rotation_euler", frame=frame)


def _shift(frame, by):
    return (frame - 1 + by) % CYCLE + 1


def animate(arm):
    pose = arm.pose.bones
    for side, offset in (("L", 0), ("R", 12)):
        for frame, (thigh, knee, toe) in LEG.items():
            f = _shift(frame, offset)
            _key(pose[f"thigh.{side}"], f, x=-thigh)
            _key(pose[f"shin.{side}"], f, x=knee)
            _key(pose[f"foot.{side}"], f, x=-toe)
        for frame, (swing, elbow) in ARM.items():
            f = _shift(frame, offset)
            _key(pose[f"upperarm.{side}"], f, x=-swing, z=4 if side == "L" else -4)
            _key(pose[f"forearm.{side}"], f, x=elbow)
    for step, mirror in ((0, 1), (12, -1)):
        for frame, (drop, yaw, roll) in BODY.items():
            f = frame + step
            hips = pose["hips"]
            hips.location = (0, drop, 0)
            hips.keyframe_insert("location", frame=f)
            _key(hips, f, y=yaw * mirror, z=roll * mirror)
            _key(pose["spine"], f, x=SPINE_LEAN, y=-yaw * mirror * SHOULDER_COUNTER, z=-roll * mirror * 0.6)
            _key(pose["head"], f, x=-SPINE_LEAN, y=yaw * mirror * 0.3)
    _close_loops(arm)
    scene = bpy.context.scene
    scene.render.fps = FPS
    scene.frame_start, scene.frame_end = 1, CYCLE
    arm.animation_data.action.name = "Walk"


def _fcurves(action):
    try:
        return list(action.fcurves)
    except AttributeError: # Blender 5: layered actions
        curves = []
        for layer in action.layers:
            for strip in layer.strips:
                for bag in strip.channelbags:
                    curves.extend(bag.fcurves)
        return curves


def _close_loops(arm):
    """Copy each curve's frame-1 key to frame 25 and make the curve repeat (Cycles modifier)."""
    for fc in _fcurves(arm.animation_data.action):
        first = min(fc.keyframe_points, key=lambda k: k.co.x)
        fc.keyframe_points.insert(CYCLE + 1, first.co.y)
        fc.modifiers.new("CYCLES")
        fc.update()


def build():
    for obj in list(bpy.data.objects):
        bpy.data.objects.remove(obj, do_unlink=True)
    arm = make_armature()
    make_body(arm)
    animate(arm)
    stage()
    return arm


def stage():
    """Ground, sun and a side camera for looking at the walk (not exported)."""
    bpy.ops.mesh.primitive_plane_add(size=8)
    bpy.context.object.name = "Ground"
    bpy.ops.object.light_add(type="SUN", rotation=(0.8, 0.2, 0.6))
    bpy.context.object.name = "Sun"
    bpy.context.object.data.energy = 3
    bpy.ops.object.camera_add(location=(4.2, -1.2, 1.0), rotation=(1.5365, 0, 1.2925))
    cam = bpy.context.object
    cam.name = "Side"
    bpy.context.scene.camera = cam


def export(folder):
    """Save pip_walk.blend and pip_walk.glb (character + Walk action only) into folder."""
    import os
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(folder, "pip_walk.blend"), copy=True)
    bpy.ops.object.select_all(action="DESELECT")
    arm = bpy.data.objects["Pip"]
    arm.select_set(True)
    for child in arm.children:
        child.select_set(True)
    bpy.ops.export_scene.gltf(
        filepath=os.path.join(folder, "pip_walk.glb"),
        use_selection=True,
        export_animations=True,
        export_animation_mode="ACTIONS",
        export_extras=True,
    )
