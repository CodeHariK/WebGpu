"""The finisher, pose to pose (80 frames at 24 fps, slow motion baked in).

  1–13  Pip, in a fighting stance, dashes in and crouches, right fist cocked
 14–20  leaps and throws a flying punch — CONTACT on frame 20
 20–38  SLOW MOTION: the punch follows through, the Grunt's head snaps back, an impact star blooms
 38–59  real speed again: the Grunt flies backwards, crashes on his back and bounces;
        Pip drops into a three-point landing
 60–80  Pip stands up and looks down at him; the Grunt lies dazed

Every key is a full pose (see finisher_rig.key_pose for the words). After keying, plant() puts the
feet (or the back) on the ground at the frames listed, and the punch is lined up with the Grunt's face.
"""
import bpy
from mathutils import Vector

from finisher_rig import centre, key_poses, line_up, plant

CONTACT, SLOW_END, END = 20, 38, 80

PIP = {
    1: {"drop": -0.08, "hips": (0, -15, 0), "spine": (10, 10, 0), "head": (-8, 5, 0),
        "arm.L": (50, 10, 110), "arm.R": (35, 10, 120), "leg.L": (15, 25, 0), "leg.R": (-12, 20, 0)},
    5: {"root": (0, -0.5, 0), "drop": -0.05, "spine": (25, 0, 0), "head": (-20, 0, 0),  # dash
        "arm.L": (-40, 10, 60), "arm.R": (45, 10, 80), "leg.L": (-35, 50, -20), "leg.R": (40, 30, 10)},
    9: {"root": (0, -1.15, 0), "drop": -0.05, "spine": (28, 0, 0), "head": (-22, 0, 0),
        "arm.L": (45, 10, 80), "arm.R": (-40, 10, 60), "leg.L": (40, 30, 10), "leg.R": (-35, 50, -20)},
    13: {"root": (0, -1.55, 0), "drop": -0.28, "hips": (10, 0, 0), "spine": (30, -25, 0), "head": (-25, 20, 0),  # wind-up
         "arm.L": (60, 15, 40), "arm.R": (-60, 20, 120), "leg.L": (55, 95, 0), "leg.R": (20, 90, 0)},
    16: {"root": (0, -1.95, 0.25), "spine": (20, -10, 0), "head": (-18, 8, 0),  # launch
         "arm.L": (40, 10, 30), "arm.R": (-30, 30, 100), "leg.L": (40, 60, 0), "leg.R": (-25, 10, -50)},
    CONTACT: {"root": (0, -2.25, 0.45), "hips": (15, 0, 0), "spine": (15, 20, 0), "head": (-25, -15, 0),
              "arm.L": (-35, 15, 40), "arm.R": (110, 0, 0), "leg.L": (60, 100, 0), "leg.R": (-35, 40, -45)},
    SLOW_END: {"root": (0, -2.42, 0.47), "hips": (15, 0, 0), "spine": (17, 24, 0), "head": (-27, -18, 0),
               "arm.L": (-40, 18, 40), "arm.R": (112, 0, 0), "leg.L": (62, 105, 0), "leg.R": (-38, 42, -50)},
    44: {"root": (0, -2.6, 0), "drop": -0.45, "hips": (25, 0, 0), "spine": (35, 0, 0), "head": (-35, 0, 0),  # landing
         "arm.L": (-45, 50, 20), "arm.R": (55, 5, 0), "leg.L": (75, 120, 0), "leg.R": (-15, 115, -60)},
    49: {"root": (0, -2.62, 0), "drop": -0.48, "hips": (25, 0, 0), "spine": (40, 0, 0), "head": (-38, 0, 0),
         "arm.L": (-48, 55, 20), "arm.R": (58, 5, 0), "leg.L": (78, 125, 0), "leg.R": (-15, 118, -60)},
    60: {"root": (0, -2.62, 0), "drop": -0.15, "hips": (5, 0, 0), "spine": (10, 0, 0), "head": (-5, 0, 0),
         "arm.L": (-5, 15, 20), "arm.R": (10, 5, 20), "leg.L": (25, 45, 0), "leg.R": (5, 40, 0)},
    72: {"root": (0, -2.62, 0), "drop": -0.02, "spine": (-3, 0, 0), "head": (12, 0, 0),
         "arm.L": (0, 8, 15), "arm.R": (0, 8, 15), "leg.L": (5, 8, 0), "leg.R": (-3, 6, 0)},
}
PIP[END] = PIP[72]
PIP_GROUNDED = {1: 0, 5: 0, 9: 0, 13: 0, 16: 0, 44: 0, 49: 0, 60: 0, 72: 0, END: 0}

GUARD = {"drop": -0.12, "spine": (25, 0, 0), "head": (-20, 0, 0),
         "arm.L": (55, 15, 105), "arm.R": (55, 15, 105), "leg.L": (10, 22, 0), "leg.R": (-8, 20, 0)}
GRUNT = {
    1: {"drop": -0.05, "spine": (22, 0, 0), "head": (-20, 0, 0),  # hunched, menacing
        "arm.L": (10, 25, 35), "arm.R": (10, 25, 35), "leg.L": (8, 15, 0), "leg.R": (-5, 12, 0)},
    8: {"drop": -0.07, "spine": (25, 5, 0), "head": (-22, -5, 0),
        "arm.L": (12, 28, 40), "arm.R": (12, 28, 40), "leg.L": (8, 15, 0), "leg.R": (-5, 12, 0)},
    14: {"drop": -0.03, "spine": (8, 0, 0), "head": (-12, 0, 0),  # notices, raises his guard
         "arm.L": (45, 20, 90), "arm.R": (40, 20, 95), "leg.L": (8, 12, 0), "leg.R": (-5, 10, 0)},
    19: GUARD,
    CONTACT: GUARD,
    SLOW_END: {"root": (0, 0.05, 0), "root_rot": (-3, 0, 0), "drop": -0.08, "spine": (5, -10, 0), "head": (-35, 10, 0),
               "arm.L": (70, 50, 30), "arm.R": (60, 45, 20), "leg.L": (5, 10, 10), "leg.R": (-10, 15, 0)},
    44: {"root": (0, 0.6, 0.35), "root_rot": (-40, 0, 0), "spine": (-15, 0, 0), "head": (-30, 0, 0),  # flying back
         "arm.L": (150, 40, 20), "arm.R": (140, 45, 25), "leg.L": (35, 25, 30), "leg.R": (20, 15, 30)},
    50: {"root": (0, 1.2, 0.2), "root_rot": (-90, 0, 0), "spine": (-5, 0, 0), "head": (-15, 0, 0),  # crash
         "arm.L": (160, 60, 15), "arm.R": (155, 55, 20), "leg.L": (45, 30, 20), "leg.R": (30, 20, 20)},
    54: {"root": (0, 1.32, 0.32), "root_rot": (-80, 0, 0), "spine": (-5, 0, 0), "head": (-12, 0, 0),  # bounce
         "arm.L": (150, 70, 30), "arm.R": (150, 70, 30), "leg.L": (60, 40, 20), "leg.R": (45, 30, 20)},
    59: {"root": (0, 1.38, 0.2), "root_rot": (-90, 0, 0), "head": (-10, 25, 0),  # out cold
         "arm.L": (165, 65, 10), "arm.R": (160, 60, 15), "leg.L": (15, 20, 30), "leg.R": (10, 15, 30)},
    72: {"root": (0, 1.38, 0.2), "root_rot": (-90, 0, 0), "head": (-5, 35, 0),
         "arm.L": (165, 68, 10), "arm.R": (160, 62, 15), "leg.L": (14, 20, 30), "leg.R": (10, 15, 30)},
}
GRUNT[END] = GRUNT[72]
GRUNT_GROUNDED = {1: 0, 8: 0, 14: 0, 19: 0, CONTACT: 0, SLOW_END: 0, 50: 0, 54: 0.1, 59: 0, 72: 0, END: 0}

GLOVE_RADIUS = 0.085


def punch_target(grunt, glove):
    """The front of the Grunt's face, on the side the glove comes from."""
    head = bpy.data.objects["Grunt.Head"]
    face = centre(head)
    toward = centre(glove) - face
    toward.z = 0
    radius = 0.24 * grunt.scale.x
    return face + toward.normalized() * (radius + GLOVE_RADIUS) + Vector((0, 0, 0.02))


def animate(pip, grunt):
    """Key both characters, plant them, line the punch up. Returns the contact point (world)."""
    key_poses(pip, PIP, "Finisher")
    key_poses(grunt, GRUNT, "Finisher_Grunt")
    plant(pip, PIP_GROUNDED)
    plant(grunt, GRUNT_GROUNDED)
    bpy.context.scene.frame_set(CONTACT)
    glove = bpy.data.objects["Glove.R"]
    target = punch_target(grunt, glove)
    line_up(pip, "Glove.R", target, [CONTACT, SLOW_END])
    scene = bpy.context.scene
    scene.render.fps = 24
    scene.frame_start, scene.frame_end = 1, END
    scene.frame_set(1)
    return target
