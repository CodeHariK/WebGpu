## One hand or foot: where it is, the surface it's on, and its step in progress. The gait
## (crawler_gait.gd) decides when and where it steps; this moves it there.
##
##   planted   it stays exactly where it was put while the body moves (the spider's rule: a
##             planted foot never slides)
##   step      a cubic Bezier from the old spot to the new one (the cultist's arc): the far
##             control point sits `lift` out along the landing surface's normal, so the foot
##             comes down onto the surface whatever its angle (floor, wall, ceiling); the near
##             one along the surface it leaves, so it peels off. Both are pushed at least
##             `clearance` from the body's centre line, so the arc swings round the body.
##             Eased in and out (smoothstep): quick in the middle, soft at both ends.
##   airborne  no arcs: it eases toward the spot the gait gives it (its relaxed pose, flailing a
##   hanging   little); `hanging` is the same on the ground: nothing in reach to hold, so it
##             hangs relaxed (following the body) instead of stretching for the air
class_name CrawlerStep
extends RefCounted

var lift := 0.18 ## how far the arc rises off the surfaces (m)
var clearance := 0.35 ## control points at least this far from the body's centre line (m)

var position := Vector3.ZERO ## where the hand/foot is now
var normal := Vector3.UP ## the surface under it (blends from old to new during a step)
var on_surface := false
var stepping := false
var airborne := false
var hanging := false
var just_planted := false ## true for the one update in which a step lands

var _t := 0.0
var _duration := 0.25
var _from := Vector3.ZERO
var _from_control := Vector3.ZERO
var _to_control := Vector3.ZERO
var _to := Vector3.ZERO
var _from_normal := Vector3.UP
var _to_normal := Vector3.UP


## Put it straight down on a spot (spawning).
func place(spot: Vector3, surface_normal: Vector3, hit: bool) -> void:
	position = spot
	normal = surface_normal
	_to_normal = surface_normal
	on_surface = hit
	stepping = false
	hanging = false


## Start a step to `spot`, taking `duration` seconds. `centre`/`up`: the body's centre and up
## (the arc keeps clear of the line through them). `arc_lift` overrides `lift` for this step
## (the gallop's lower, quicker swings); < 0 uses `lift`.
func step_to(spot: Vector3, surface_normal: Vector3, hit: bool, duration: float, centre: Vector3, up: Vector3, arc_lift := -1.0) -> void:
	var height := lift if arc_lift < 0.0 else arc_lift
	_from = position
	_to = spot
	_from_normal = normal
	_to_normal = surface_normal
	_from_control = _clear(position + _from_normal * height, centre, up)
	_to_control = _clear(spot + surface_normal * height, centre, up)
	_duration = maxf(duration, 0.05)
	_t = 0.0
	stepping = true
	hanging = false
	on_surface = hit


## Nothing to hold: let go and hang relaxed until the gait finds a foothold again.
func hang() -> void:
	hanging = true
	stepping = false
	on_surface = false


## 0..1 through the step in progress (0 when not stepping).
func progress() -> float:
	return _t if stepping else 0.0


## Where it is planted, or where the step in progress lands.
func landing() -> Vector3:
	return _to if stepping else position


## The arc of the step in progress (for the debug drawing): `count` + 1 points, from to to.
func arc(count: int) -> PackedVector3Array:
	var points := PackedVector3Array()
	for i in count + 1:
		var t := smoothstep(0.0, 1.0, float(i) / count)
		points.append(_from.bezier_interpolate(_from_control, _to_control, _to, t))
	return points


## Leave the ground (true) or come back to it (false; the gait then steps every limb down).
func set_airborne(flying: bool) -> void:
	airborne = flying
	hanging = false
	if flying:
		stepping = false
		on_surface = false


## Advance the step, or (in the air / hanging) ease toward `relaxed` at `relax_rate` per second.
func update(delta: float, relaxed: Vector3, relax_rate := 5.0) -> void:
	just_planted = false
	if airborne or hanging:
		position = position.lerp(relaxed, 1.0 - exp(-relax_rate * delta))
		return
	if not stepping:
		return
	_t = minf(_t + delta / _duration, 1.0)
	var eased := smoothstep(0.0, 1.0, _t)
	position = _from.bezier_interpolate(_from_control, _to_control, _to, eased)
	normal = _from_normal.slerp(_to_normal, eased) if not _from_normal.is_equal_approx(-_to_normal) else _to_normal
	if _t >= 1.0:
		stepping = false
		just_planted = true


## Push a control point out to at least `clearance` from the line through `centre` along `up`.
func _clear(point: Vector3, centre: Vector3, up: Vector3) -> Vector3:
	var axis := centre + up * (point - centre).dot(up)
	var out := point - axis
	if out.length() >= clearance:
		return point
	if out.length() < 1e-4:
		return point
	return axis + out.normalized() * clearance
