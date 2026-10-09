"""Rig helpers for the finisher: Pip's skeleton under any name, rigid parts, palette materials,
full-pose keyframes, and two fix-up passes (plant feet on the ground, line a hit up with its target).

Rotation conventions measured on this rig (degrees, bone-local X): thigh / upper arm + swings back,
shin + bends the knee, foot + points the toe down, forearm + bends the elbow, hips / spine / head
+ lean forward. Hips, spine and head point up, so their local Y is yaw and local Z is roll.
The root points forward-back (local Y), so its local Z is yaw and local Y is roll.
"""
import math

import bmesh
import bpy
from mathutils import Matrix, Vector

from build_walker import BONES


def palette_material(prefix, name, rgb, glow=0.0):
    key = f"M_{prefix}_{name}"
    mat = bpy.data.materials.get(key)
    if mat:
        return mat
    mat = bpy.data.materials.new(key)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*rgb, 1)
    bsdf.inputs["Roughness"].default_value = 0.6
    if glow:
        bsdf.inputs["Emission Color"].default_value = (*rgb, 1)
        bsdf.inputs["Emission Strength"].default_value = glow
    mat.diffuse_color = (*rgb, 1)
    return mat


def make_rig(name):
    """An armature object `name` with Pip's bones, at the origin facing −Y. Move it with place() after the body is on."""
    data = bpy.data.armatures.new(f"{name}Rig")
    arm = bpy.data.objects.new(name, data)
    bpy.context.scene.collection.objects.link(arm)
    data.display_type = "STICK"
    arm.show_in_front = True
    bpy.context.view_layer.objects.active = arm
    window = bpy.context.window_manager.windows[0]
    with bpy.context.temp_override(window=window, active_object=arm, object=arm, selected_objects=[arm]):
        bpy.ops.object.mode_set(mode="EDIT")
        for bone_name, (head, tail, parent) in BONES.items():
            bone = data.edit_bones.new(bone_name)
            bone.head, bone.tail = head, tail
            bone.roll = 0.0
            if parent:
                bone.parent = data.edit_bones[parent]
                bone.use_connect = Vector(head) == data.edit_bones[parent].tail
        bpy.ops.object.mode_set(mode="OBJECT")
    for pb in arm.pose.bones:
        pb.rotation_mode = "XYZ"
    return arm


def part(arm, name, bone, mat, build):
    """A mesh built in rest position (world space, rig at the origin), parented to `bone` without moving."""
    bm = bmesh.new()
    build(bm)
    mesh = bpy.data.meshes.new(name)
    bm.to_mesh(mesh)
    bm.free()
    for poly in mesh.polygons:
        poly.use_smooth = True
    mesh.materials.append(mat)
    obj = bpy.data.objects.new(name, mesh)
    bpy.context.scene.collection.objects.link(obj)
    b = arm.data.bones[bone]
    tail = arm.matrix_world @ b.matrix_local @ Matrix.Translation((0, b.length, 0))
    obj.parent = arm
    obj.parent_type = "BONE"
    obj.parent_bone = bone
    obj.matrix_parent_inverse = tail.inverted()
    return obj


def place(arm, location, turn_degrees=0.0, scale=1.0):
    arm.location = location
    arm.rotation_euler = (0, 0, math.radians(turn_degrees))
    arm.scale = (scale, scale, scale)


# --- posing ------------------------------------------------------------------------------------

def _rot(pb, frame, x=0.0, y=0.0, z=0.0):
    pb.rotation_euler = tuple(math.radians(v) for v in (x, y, z))
    pb.keyframe_insert("rotation_euler", frame=frame)


def _loc(pb, frame, xyz):
    pb.location = xyz
    pb.keyframe_insert("location", frame=frame)


def key_pose(arm, frame, pose):
    """Key every bone at `frame` from a pose in plain words; anything left out is the rest pose.

    root (x, y, z): metres in the character's own axes (−Y is forward)
    root_rot / hips / spine / head (pitch, yaw, roll): degrees, + pitch leans forward
    drop: hips up (+) or down (−), metres
    arm.L / arm.R (forward, out, elbow) · leg.L / leg.R (forward, knee, toe up from the ground)
    """
    pb = arm.pose.bones
    root_pitch, root_yaw, root_roll = pose.get("root_rot", (0, 0, 0))
    hips = pose.get("hips", (0, 0, 0))
    _loc(pb["root"], frame, pose.get("root", (0, 0, 0)))
    _rot(pb["root"], frame, root_pitch, root_roll, root_yaw)
    _loc(pb["hips"], frame, (0, pose.get("drop", 0.0), 0))
    _rot(pb["hips"], frame, *hips)
    _rot(pb["spine"], frame, *pose.get("spine", (0, 0, 0)))
    _rot(pb["head"], frame, *pose.get("head", (0, 0, 0)))
    for side, out_sign in (("L", 1), ("R", -1)):
        forward, out, elbow = pose.get(f"arm.{side}", (0, 0, 0))
        _rot(pb[f"upperarm.{side}"], frame, x=-forward, z=out * out_sign)
        _rot(pb[f"forearm.{side}"], frame, x=elbow)
        forward, knee, toe_up = pose.get(f"leg.{side}", (0, 0, 0))
        _rot(pb[f"thigh.{side}"], frame, x=-forward)
        _rot(pb[f"shin.{side}"], frame, x=knee)
        # every X rotation above the foot adds up; cancel it so toe_up is measured from the ground
        _rot(pb[f"foot.{side}"], frame, x=-toe_up - root_pitch - hips[0] + forward - knee)


def key_poses(arm, poses, action_name):
    for frame, pose in poses.items():
        key_pose(arm, frame, pose)
    arm.animation_data.action.name = action_name


# --- fix-up passes -----------------------------------------------------------------------------

def _to_root(arm, world_vector):
    """A world-space offset as a root-bone location offset (the root's rest frame is the rig's own axes)."""
    return arm.matrix_world.to_3x3().inverted() @ world_vector


def _nudge_root(arm, frame, world_shift):
    bpy.context.scene.frame_set(frame)
    root = arm.pose.bones["root"]
    root.location = root.location + _to_root(arm, world_shift)
    root.keyframe_insert("location", frame=frame)


def lowest_point(arm):
    low = math.inf
    for obj in arm.children:
        if obj.type == "MESH":
            mw = obj.matrix_world
            low = min(low, min((mw @ v.co).z for v in obj.data.vertices))
    return low


def plant(arm, frames):
    """At each frame → lift, move the root so the lowest point of the body sits `lift` above z = 0."""
    for frame, lift in frames.items():
        bpy.context.scene.frame_set(frame)
        _nudge_root(arm, frame, Vector((0, 0, lift - lowest_point(arm))))


def centre(obj):
    return obj.matrix_world @ (sum((Vector(c) for c in obj.bound_box), Vector()) / 8)


def line_up(arm, part_name, target, frames):
    """Shift the root by the same amount at every frame in `frames` so `part_name` is at `target` on the first."""
    bpy.context.scene.frame_set(frames[0])
    shift = target - centre(bpy.data.objects[part_name])
    for frame in frames:
        _nudge_root(arm, frame, shift)
    return shift
