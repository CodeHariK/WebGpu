## The crawler's torso frame: where the chest is and which way it faces, following the ball.
## Ported from LimbPlacementController.UpdateBodyPositions in the original.
##
##   position   on the ball's centre across (x, z); height eased toward it, dipping a little
##              for every limb in the middle of a step (so the body bobs as it walks)
##   rotation   -Z along the way the ball last moved, Y along the surface it's on (floor, wall
##              or ceiling); eased so the body turns smoothly
## The cultist's chest bone is laid along this frame (cultist_rig.gd), and limbs use it to keep
## to their own side (limb.gd).
extends RefCounted

var follow_height := 12.0  ## how fast the height eases (per second)
var turn_speed := 8.0
var dip_per_step := 0.2  ## metres lower for each limb mid-step

var frame := Transform3D.IDENTITY  ## X right, Y up (away from the surface), -Z forward


func _init(at: Vector3) -> void:
	frame.origin = at


## `up`: the surface normal (UP on the floor). `stepping`: limbs mid-step right now.
func update(delta: float, centre: Vector3, heading: Vector3, up: Vector3, stepping: int) -> void:
	var goal := centre - frame.basis.y * dip_per_step * stepping
	frame.origin.x = centre.x
	frame.origin.z = centre.z
	frame.origin.y = lerpf(frame.origin.y, goal.y, minf(delta * follow_height, 1.0))
	if absf(heading.dot(up)) > 0.99:
		return  # heading straight into or out of the surface: keep the last facing
	var facing := Basis.looking_at(heading, up)
	var turned := frame.basis.get_rotation_quaternion().slerp(facing.get_rotation_quaternion(), minf(delta * turn_speed, 1.0))
	frame.basis = Basis(turned)
