"""The set: ground, sun, a camera that pushes in for the slow-motion hit, and a cartoon impact star."""
import math

import bmesh
import bpy
from mathutils import Vector

from finisher_moves import CONTACT, SLOW_END
from finisher_rig import palette_material

# frame → (camera position, look-at point); None look-at = the contact point
CAMERA_KEYS = {
    1: ((5.5, 1.8, 1.4), (0, 1.6, 0.9)),
    12: ((4.2, 1.6, 1.3), (0, 0.9, 1.0)),
    19: ((3.2, 1.5, 1.6), None),
    CONTACT: ((2.7, 1.0, 1.9), None),  # push in for the hit
    SLOW_END: ((2.4, -0.5, 1.8), None),  # slow orbit while time crawls
    46: ((4.4, 0.2, 1.4), (0, -1.0, 0.6)),
    72: ((5.2, 0.0, 1.6), (0, -1.2, 0.5)),
}


def ground_and_sun():
    bpy.ops.mesh.primitive_plane_add(size=12)
    ground = bpy.context.object
    ground.name = "Ground"
    ground.data.materials.append(palette_material("Set", "ground", (0.35, 0.42, 0.3)))
    bpy.ops.object.light_add(type="SUN", rotation=(0.8, 0.2, 0.6))
    bpy.context.object.name = "Sun"
    bpy.context.object.data.energy = 3


def camera(contact):
    target = bpy.data.objects.new("CamTarget", None)
    bpy.context.scene.collection.objects.link(target)
    data = bpy.data.cameras.new("FinisherCam")
    data.lens = 40
    cam = bpy.data.objects.new("FinisherCam", data)
    bpy.context.scene.collection.objects.link(cam)
    track = cam.constraints.new("TRACK_TO")
    track.target = target
    track.track_axis, track.up_axis = "TRACK_NEGATIVE_Z", "UP_Y"
    for frame, (position, look_at) in CAMERA_KEYS.items():
        cam.location = position
        cam.keyframe_insert("location", frame=frame)
        target.location = look_at if look_at else contact
        target.keyframe_insert("location", frame=frame)
    bpy.context.scene.camera = cam
    return cam


def impact_star(contact, cam):
    """A flat eight-point star that faces the camera, pops on CONTACT and fades out by SLOW_END."""
    bm = bmesh.new()
    points = []
    for i in range(16):
        angle = math.tau * i / 16
        radius = 0.32 if i % 2 == 0 else 0.13
        points.append(bm.verts.new((radius * math.cos(angle), radius * math.sin(angle), 0)))
    bm.faces.new(points)
    mesh = bpy.data.meshes.new("ImpactStar")
    bm.to_mesh(mesh)
    bm.free()
    mesh.materials.append(palette_material("Set", "impact", (1.0, 0.85, 0.2), glow=3.0))
    star = bpy.data.objects.new("ImpactStar", mesh)
    bpy.context.scene.collection.objects.link(star)
    star.location = contact
    face = star.constraints.new("TRACK_TO")
    face.target = cam
    face.track_axis, face.up_axis = "TRACK_Z", "UP_Y"
    for frame, size in ((CONTACT - 1, 0.0), (CONTACT, 1.0), (CONTACT + 8, 1.3), (SLOW_END, 0.0)):
        star.scale = (size, size, size)
        star.keyframe_insert("scale", frame=frame)
    return star


def build_set(contact):
    ground_and_sun()
    cam = camera(contact)
    impact_star(contact, cam)
