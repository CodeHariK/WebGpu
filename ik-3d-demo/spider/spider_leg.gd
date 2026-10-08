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

## A half circle from (0, 0) over the top to (1, 0), in 12 straight pieces.
const HALF_CIRCLE := [Vector2(0.000, 0.000), Vector2(0.017, 0.259), Vector2(0.067, 0.500), Vector2(0.146, 0.707), Vector2(0.250, 0.866), Vector2(0.371, 0.966), Vector2(0.500, 1.000), Vector2(0.629, 0.966), Vector2(0.750, 0.866), Vector2(0.854, 0.707), Vector2(0.933, 0.500), Vector2(0.983, 0.259), Vector2(1.000, 0.000)]
## Step paths: (along the stride 0..1, up 0..1 of step_height) points, walked in straight lines at
## constant speed — no easing. Empty = the smooth (eased, sine-lifted) arc.
const STEP_SHAPES := {
	"arc": [],
	"circle": HALF_CIRCLE, # truly round: its height is half the stride, whatever step_height is
	"ellipse": HALF_CIRCLE, # the same curve squashed to step_height
	"square": [Vector2(0, 0), Vector2(0, 1), Vector2(1, 1), Vector2(1, 0)],
	"octagon": [Vector2(0, 0), Vector2(0, 0.6), Vector2(0.15, 1), Vector2(0.85, 1), Vector2(1, 0.6), Vector2(1, 0)],
	"trapezoid": [Vector2(0, 0), Vector2(0.15, 1), Vector2(0.85, 1), Vector2(1, 0)],
	"triangle": [Vector2(0, 0), Vector2(0.5, 1), Vector2(1, 0)],
	"hexagon": [Vector2(0, 0), Vector2(-0.08, 0.55), Vector2(0.2, 1), Vector2(0.8, 1), Vector2(1.08, 0.55), Vector2(1, 0)],
	"sawtooth": [Vector2(0, 0), Vector2(0, 1), Vector2(1, 0)], # straight up, then a long slope down onto the spot
	"stab": [Vector2(0, 0), Vector2(0.85, 1), Vector2(1, 0)], # a long rise forward, then straight down: a stab
}
const ROUND_SHAPES: Array[String] = ["circle"] ## height = half the stride (not step_height)
const SMOOTH_SHAPES: Array[String] = ["circle", "ellipse"] ## many points: no pauses at them

var target: Marker3D ## the IK target this leg moves
var planted: Vector3 ## world-space foot position while on the ground
var stepping := false
var step_from: Vector3 ## current/last step: start, end and progress (0..1)
var step_to: Vector3
var progress := 0.0
var roundness := 0.0 ## 0 = hop (sine lift) … 1 = windmill swing (see arc_point)
var box_steps := false ## robot step: straight up, straight across, straight down
## Polygon step path (see STEP_SHAPES), walked at constant speed. Overrides box_steps and the arc.
var polygon := PackedVector2Array()
var corner_pause := 0.0 ## share of the step spent dead still at each inner corner (a servo settling)
var polygon_round := false ## the polygon's height is half the stride instead of step_height (a circle)
## A real spider's step: a quick flick up and forward that eases out onto the landing spot (most
## of the travel early, slowing as it lands), the lift peaking early. Overrides the other shapes.
var quick := false
## Noise on the step path: metres of wobble (all three axes), zero at take-off and landing (it
## rises and falls with sin(π·t)) so the foot still lifts off and lands exactly. Each step gets
## its own random wobble. 0 = a clean curve.
var noise_amount := 0.0
var noise_rate := 3.0 ## wobbles per step

static var _noise := FastNoiseLite.new()
var _noise_seed := 0.0 ## where in the noise this step reads (new each step)
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
	_noise_seed = randf() * 1000.0


## Point on the step arc at progress `t` (0..1).
##   roundness 0: eased slide plus a sine lift (a hop).
##   roundness 1: a windmill swing — the lift grows to half the stride (a half circle over the
##     top), and the foot first pulls back behind where it was (pushing off), then reaches past
##     the landing spot and comes back onto it (a grab). Same start and end points either way.
func arc_point(t: float, step_height: float) -> Vector3:
	return _clean_point(t, step_height) + _wobble(t)


# The step path without noise.
func _clean_point(t: float, step_height: float) -> Vector3:
	if quick:
		return step_from.lerp(step_to, _ease_out(t)) + Vector3.UP * sin(PI * pow(t, 0.6)) * step_height
	if polygon.size() >= 2:
		var point := _polygon_point(t, step_height)
		return step_from.lerp(step_to, point.x) + Vector3.UP * point.y
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


# Where along the polygon the foot is at progress t: (share of the stride, metres up). The legs
# of the polygon are measured in metres (stride across, step_height up), so the foot moves at one
# constant speed; with corner_pause it stops dead at each inner corner for that share of the step.
func _polygon_point(t: float, step_height: float) -> Vector2:
	var stride := maxf(Vector2(step_to.x - step_from.x, step_to.z - step_from.z).length(), 0.01)
	if polygon_round:
		step_height = stride * 0.5
	var corners := polygon.size()
	var lengths: Array[float] = []
	var total := 0.0
	for i in corners - 1:
		var d := (polygon[i + 1] - polygon[i]) * Vector2(stride, step_height)
		lengths.append(d.length())
		total += d.length()
	var pauses := corner_pause * (corners - 2)
	var moving := maxf(1.0 - pauses, 0.05) # share of the step spent moving
	var clock := 0.0 # walk the timeline: move, pause, move, pause, … move
	for i in corners - 1:
		var span := moving * lengths[i] / maxf(total, 1e-5)
		if t <= clock + span:
			var k := (t - clock) / maxf(span, 1e-5)
			return _scaled(polygon[i].lerp(polygon[i + 1], k), step_height)
		clock += span
		if i < corners - 2:
			if t <= clock + corner_pause:
				return _scaled(polygon[i + 1], step_height)
			clock += corner_pause
	return _scaled(polygon[corners - 1], step_height)


# The step's noise at progress t: three smooth channels, faded in and out by sin(π·t).
func _wobble(t: float) -> Vector3:
	if noise_amount <= 0.0:
		return Vector3.ZERO
	var x := t * noise_rate * 10.0
	var wobble := Vector3(
		_noise.get_noise_2d(x, _noise_seed),
		_noise.get_noise_2d(x, _noise_seed + 300.0),
		_noise.get_noise_2d(x, _noise_seed + 600.0))
	return wobble * noise_amount * sin(PI * t) * 2.0 # FastNoiseLite stays mostly within ±0.5


static func _ease_out(t: float) -> float:
	return 1.0 - pow(1.0 - t, 3.0)


static func _scaled(corner: Vector2, step_height: float) -> Vector2:
	return Vector2(corner.x, corner.y * step_height)


## Use a STEP_SHAPES path by name ("arc" = the smooth arc); `pause` = corner_pause for the
## sharp shapes (round ones never pause).
func set_shape(shape: String, pause: float) -> void:
	polygon = PackedVector2Array(STEP_SHAPES.get(shape, []))
	polygon_round = shape in ROUND_SHAPES
	corner_pause = 0.0 if shape in SMOOTH_SHAPES else pause


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
		var along := smoothstep(0.0, 1.0, progress)
		if quick:
			along = _ease_out(progress)
		elif polygon.size() >= 2:
			along = _polygon_point(progress, 0.0).x
		return step_from.lerp(step_to, along)
	return planted
