## One limb of the crawler: where its hand or foot is right now (the IK target) and how it gets
## from one planted spot to the next. Ported from Limb.gd in the original. limb_gait.gd decides
## WHEN a limb steps and WHERE to; this file only moves the point.
##
##   step      a cubic Bezier from where the limb is to the new spot. The far control point sits
##             control_offset out along the landing surface's normal (so it comes down onto the
##             surface), and both control points are pushed at least control_reach away from
##             the body's centre line (so the arc swings round the body, not through it).
##             Leaving a surface, the near control point goes back the way it came first, a
##             little scoop, like theirs. blend_speed is steps per second (hands 3.5, feet 4.5).
##   side      while stepping, the point stays at least min_side out on its own side of the
##             torso, so limbs never cross under the body
##   airborne  no arcs: the point chases the flailing spot limb_gait.gd hands it
extends RefCounted

var is_foot: bool
var is_left: bool
var blend_speed := 3.5
var control_offset := 0.4
var control_reach := 1.5
var min_side := 0.3

var position := Vector3.ZERO  ## where the hand or foot is now
var traveling := false
var airborne := false
var on_surface := false  ## the spot it's heading for (or planted on) is on something
var fly_phase := 0.0  ## each limb flails out of step with the others

var _t := 0.0
var _from := Vector3.ZERO
var _from_control := Vector3.ZERO
var _to_control := Vector3.ZERO
var _to := Vector3.ZERO


func _init(foot: bool, left: bool) -> void:
	is_foot = foot
	is_left = left
	if foot:  # their prefab's foot settings
		blend_speed = 4.5
		control_offset = 0.25
		min_side = 0.4


## Put the limb straight down on a spot (spawning).
func place(spot: Vector3, normal: Vector3, hit: bool) -> void:
	position = spot
	_to = spot
	_to_control = spot + normal * control_offset
	on_surface = hit
	traveling = false


## Start a step to `spot` (surface `normal`). `centre`: the ball's centre.
func step_to(spot: Vector3, normal: Vector3, hit: bool, centre: Vector3) -> void:
	var to_control := spot + normal * control_offset
	var from_control := position + (_to_control - _to)  # lift off the way the last step came down
	var axis := Vector3(centre.x, to_control.y, centre.z)
	if to_control.distance_to(axis) < control_reach:
		to_control = axis + (to_control - axis).normalized() * control_reach
	if on_surface:
		from_control = position + (position - to_control).normalized() * control_offset
	elif from_control.distance_to(axis) < control_reach:
		from_control = axis + (from_control - axis).normalized() * control_reach
	_from = position
	_from_control = from_control
	_to_control = to_control
	_to = spot
	on_surface = hit
	_t = 0.0
	traveling = true


## Leave the ground (true, with a random flail phase) or land (false).
func set_airborne(flying: bool) -> void:
	if flying and not airborne:
		traveling = false
		fly_phase = randf() * 10.0
	airborne = flying


## Move the point along. `torso`: the body's frame (X right, Y up, -Z forward).
## `fly_to`: where to flail toward while airborne.
func update(delta: float, torso: Transform3D, fly_to: Vector3) -> void:
	if airborne:
		position = position.lerp(fly_to, minf(blend_speed * delta, 1.0))
		return
	if not traveling:
		return
	_t = minf(_t + blend_speed * delta, 1.0)
	position = _keep_side(_from.bezier_interpolate(_from_control, _to_control, _to, _t), torso)
	if _t >= 1.0:
		traveling = false


func _keep_side(point: Vector3, torso: Transform3D) -> Vector3:
	var local := torso.affine_inverse() * point
	local.x = minf(local.x, -min_side) if is_left else maxf(local.x, min_side)
	return torso * local
