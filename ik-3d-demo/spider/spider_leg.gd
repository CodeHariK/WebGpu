## One leg's gait: the foot stays planted on the ground while the body moves; when it falls too
## far behind its "home" spot it steps — an arc from the old spot to a new one.
##
##   home  = where this foot would rest right now (under the body, plus a bit ahead when moving)
##   plant = where the foot actually is on the ground
##   step  = when |plant - home| > step_distance: lerp plant → home over step_time,
##           lifted by sin(π·t) · step_height so the foot arcs instead of sliding
##
## The Spider decides *whether* a leg may step (alternating diagonal pairs); this class only
## knows *how*.
class_name SpiderLeg
extends RefCounted

var target: Marker3D ## the IK target this leg moves
var planted: Vector3 ## world-space foot position while on the ground
var stepping := false
var step_from: Vector3 ## current/last step: start, end and progress (0..1)
var step_to: Vector3
var progress := 0.0
var roundness := 0.0 ## 0 = hop (sine lift) … 1 = windmill swing (see arc_point)
var box_steps := false ## robot step: straight up, straight across, straight down
var servo: SecondOrder ## robot foot: the target follows the gait through this spring (null = exact)
var just_planted := false ## true for the one update in which a step landed

## Gesture override (tapping, waving): while on, the foot goes to `override_position` and the
## gait leaves this leg alone. On release it steps straight back to the ground.
var override := false
var override_position: Vector3
var needs_return := false


func _init(foot_target: Marker3D) -> void:
	target = foot_target
	planted = target.global_position


## True when the foot is far enough from `home` that it should step (and isn't stepping already).
func wants_step(home: Vector3, step_distance: float) -> bool:
	return not override and not stepping and planted.distance_to(home) > step_distance


func start_step(home: Vector3) -> void:
	needs_return = false
	stepping = true
	step_from = planted
	step_to = home
	progress = 0.0


## Point on the step arc at progress `t` (0..1).
##   roundness 0: eased slide plus a sine lift (a hop).
##   roundness 1: a windmill swing — the lift grows to half the stride (a half circle over the
##     top), and the foot first pulls back behind where it was (pushing off), then reaches past
##     the landing spot and comes back onto it (a grab). Same start and end points either way.
func arc_point(t: float, step_height: float) -> Vector3:
	if box_steps:
		return _box_point(t, step_height)
	var along := smoothstep(0.0, 1.0, t)
	var lift := step_height
	if roundness > 0.0:
		var stride := (step_to - step_from) * Vector3(1, 0, 1)
		along -= sin(TAU * t) * 0.12 * roundness # + early: pull back; − late: overshoot forward
		lift = lerpf(step_height, maxf(step_height, stride.length() * 0.5), roundness)
	return step_from.lerp(step_to, along) + Vector3.UP * sin(PI * t) * lift


# A robot's step in three straight moves: lift (first quarter), carry across (middle half),
# put down (last quarter). Sharp corners — the servo spring is what rounds and overshoots them.
func _box_point(t: float, step_height: float) -> Vector3:
	var along := clampf((t - 0.25) / 0.5, 0.0, 1.0)
	var lift := minf(t / 0.25, 1.0) * minf((1.0 - t) / 0.25, 1.0)
	return step_from.lerp(step_to, along) + Vector3.UP * lift * step_height


## Advances a step in progress and moves the IK target. Call every physics frame.
func update(delta: float, step_time: float, step_height: float) -> void:
	just_planted = false
	var goal: Vector3
	if override:
		goal = override_position
	elif stepping:
		progress = minf(progress + delta / step_time, 1.0)
		goal = arc_point(progress, step_height)
		if progress >= 1.0:
			stepping = false
			planted = step_to
			just_planted = true
	else:
		goal = planted
	target.global_position = servo.update(delta, goal) if servo != null else goal


## Gait off: glue the foot to its home spot every frame (feet slide like ice skates).
func snap_to(home: Vector3) -> void:
	stepping = false
	planted = home
	target.global_position = home
	if servo != null:
		servo.reset(home)


## Take the foot over for a gesture; it starts from wherever the foot is now.
func begin_override() -> void:
	if override:
		return
	override = true
	stepping = false
	override_position = target.global_position


## Hand the foot back to the gait: it steps from where the gesture left it back to the ground.
func release_override() -> void:
	if not override:
		return
	override = false
	planted = target.global_position
	needs_return = true


## Where this leg supports the body on the ground: the planted spot, or for a foot in the air,
## the ground path under it (start → end, without the lift). The body pose is fitted to these,
## so lifting a foot for a step or a gesture doesn't tip the body.
func support_point() -> Vector3:
	if stepping:
		return step_from.lerp(step_to, smoothstep(0.0, 1.0, progress))
	return planted
