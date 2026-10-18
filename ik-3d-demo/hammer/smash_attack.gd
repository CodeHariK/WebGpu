## KingClown's jump smash, as a pure function of time: sample(t) says where his feet are, which
## way the hammer points, how much he leans and squashes at any moment t of the attack — so the
## demo can play it, pause it, scrub it and replay it exactly (no state carried frame to frame).
##
## The attack, from a snapshot taken when it starts (feet, facing, the robot's head):
##   crouch   he sinks (squash) and the hammer drops low behind him (wind-up)
##   rise     he jumps on a Bezier arc; the hammer comes up and cocks behind his head, he arches back
##   smash    the last `swing_time` of the rise: the hammer whips over the top and down onto the
##            robot's head (fast at the end), his body folds forward
##   hit      `hitstop` seconds frozen at the contact (the robot flattened)
##   recover  the hammer bounces back off the head, he hops back and lands (squash)
##   settle   the hammer goes back onto his shoulder
##
## Where he must be at the contact is worked out backwards from the robot's head: the hammer head
## is `reach` from his shoulders along the hammer's angle, so his shoulders sit `reach` back along
## `contact_angle` from the head — the jump arc ends exactly there.
##
## The hammer's angle θ is measured in his swing plane (forward / up): 0° straight ahead, 90°
## straight up, 180° straight behind; the plane sits `side` to his right so it clears his head.
class_name SmashAttack
extends RefCounted

## Everything sample() returns for one moment.
class Pose:
	var feet := Vector3.ZERO
	var forward := Vector3.FORWARD
	var angle := 0.0 ## the hammer's angle in the swing plane (radians, see the header)
	var lean := 0.0 ## radians the upper body tips forward (negative: back)
	var squash := 1.0 ## height scale (1: none; below: squashed, above: stretched)
	var airborne := false
	var rising := false
	var since_hit := -1.0 ## seconds since the contact (−1: not yet); the hit-stop counts
	var phase := ""

var crouch_time := 0.2
var rise_time := 0.55 ## takeoff → contact
var swing_time := 0.16 ## the smash, the last part of the rise
var hitstop := 0.1
var recover_time := 0.45 ## contact → landed
var settle_time := 0.35 ## landed → hammer back on the shoulder
var jump_height := 1.6 ## the arc's top, this far above the higher of its two ends
var reach := 1.6 ## m from his shoulders to the hammer's head
var side := 0.18 ## the swing plane, this far to his right
var carry_angle := 140.0 ## degrees: resting on his shoulder
var wind_angle := 205.0 ## crouch: low behind
var cock_angle := 160.0 ## rising: up behind his head
var contact_angle := -35.0 ## the smash lands with the hammer this far below his shoulders
var rebound := 30.0 ## degrees the hammer bounces back up off the head
var lean_back := 0.35 ## radians he arches back while rising
var lean_smash := 0.6 ## radians he folds forward into the smash
var crouch_squash := 0.22
var stretch := 0.15 ## takeoff stretch
var land_squash := 0.2
var bounce_height := 0.5 ## the hop back off the hit
var land_back := 0.7 ## he lands this far back from where he hit (m)

var shoulder_height := 1.3 ## his shoulders above his feet (the body measures it)

var _start := Vector3.ZERO
var _forward := Vector3.FORWARD
var _head := Vector3.ZERO ## the top of the robot's head: where the hammer lands


## The tuning sliders: [var name, min, max, step].
func tunables() -> Array:
	return [
		["crouch_time", 0.05, 0.6, 0.01], ["rise_time", 0.2, 1.2, 0.01], ["swing_time", 0.05, 0.5, 0.01],
		["hitstop", 0.0, 0.4, 0.01], ["recover_time", 0.15, 1.0, 0.01], ["settle_time", 0.1, 1.0, 0.01],
		["jump_height", 0.3, 4.0, 0.05], ["reach", 1.0, 2.4, 0.05], ["side", 0.0, 0.6, 0.01],
		["carry_angle", 90.0, 200.0, 1.0], ["wind_angle", 150.0, 260.0, 1.0], ["cock_angle", 100.0, 220.0, 1.0],
		["contact_angle", -80.0, 20.0, 1.0], ["rebound", 0.0, 90.0, 1.0],
		["lean_back", 0.0, 0.8, 0.01], ["lean_smash", 0.0, 1.0, 0.01],
		["crouch_squash", 0.0, 0.5, 0.01], ["stretch", 0.0, 0.5, 0.01], ["land_squash", 0.0, 0.5, 0.01],
		["bounce_height", 0.0, 2.0, 0.05], ["land_back", 0.0, 2.0, 0.05],
	]


## Take the snapshot the attack plays from: his feet, and the top of the robot's head.
func aim(
		feet: Vector3,
		head_top: Vector3
) -> void:
	_start = feet
	_head = head_top
	var flat := Vector3(head_top.x - feet.x, 0.0, head_top.z - feet.z)
	_forward = flat.normalized() if flat.length() > 0.05 else Vector3.FORWARD


## A standing pose at `feet` facing `forward`: hammer on the shoulder (walking about between attacks).
func stand(
		feet: Vector3,
		forward: Vector3
) -> Pose:
	var pose := Pose.new()
	pose.feet = feet
	pose.forward = forward
	pose.angle = deg_to_rad(carry_angle)
	pose.phase = "free"
	return pose


## Seconds from the start to the contact, and to the end.
func contact_time() -> float:
	return crouch_time + rise_time


func length() -> float:
	return contact_time() + hitstop + recover_time + settle_time


## Where his feet are at the contact (worked back from the robot's head).
func contact_feet() -> Vector3:
	return _head - direction(_forward, deg_to_rad(contact_angle)) * reach - right_of(_forward) * side - Vector3.UP * shoulder_height


## Where he lands after the hop back.
func landing() -> Vector3:
	var at := contact_feet() - _forward * land_back
	return Vector3(at.x, _start.y, at.z)


## The hammer's direction (from his shoulders) at angle θ in the swing plane of `forward`.
static func direction(
		forward: Vector3,
		angle: float
) -> Vector3:
	return forward * cos(angle) + Vector3.UP * sin(angle)


## The way the hammer's head moves as θ grows (its face looks along this).
static func swing_of(
		forward: Vector3,
		angle: float
) -> Vector3:
	return -forward * sin(angle) + Vector3.UP * cos(angle)


## His right, facing `forward`.
static func right_of(forward: Vector3) -> Vector3:
	return forward.cross(Vector3.UP).normalized()


## Between his shoulders at `pose` (the swing's centre; it squashes with him).
func shoulders(pose: Pose) -> Vector3:
	return pose.feet + Vector3.UP * shoulder_height * pose.squash


## The point `distance` along the hammer from his shoulders at `pose` (reach: the head's centre).
func hammer_point(
		pose: Pose,
		distance: float
) -> Vector3:
	return shoulders(pose) + right_of(pose.forward) * side + direction(pose.forward, pose.angle) * distance


func head_top() -> Vector3:
	return _head


## Everything at time t (seconds from the start; clamped to the attack).
func sample(t: float) -> Pose:
	var pose := Pose.new()
	pose.forward = _forward
	t = clampf(t, 0.0, length())
	var hit := contact_time()
	pose.since_hit = t - hit if t >= hit else -1.0
	var m := t if t < hit else (hit if t < hit + hitstop else t - hitstop) # motion time: frozen in the hit-stop
	var takeoff := crouch_time
	var swing := minf(swing_time, rise_time * 0.9)
	var swing_start := hit - swing
	var landed := hit + recover_time
	var done := landed + settle_time
	var carry := deg_to_rad(carry_angle)
	var cock := deg_to_rad(cock_angle)
	var contact := deg_to_rad(contact_angle)
	if m < takeoff: # crouch
		var u := _smooth(m / crouch_time)
		pose.phase = "crouch"
		pose.feet = _start
		pose.angle = lerpf(carry, deg_to_rad(wind_angle), u)
		pose.lean = 0.25 * u
		pose.squash = 1.0 - crouch_squash * u
	elif m < hit: # rise (and the smash at its end)
		var u := (m - takeoff) / rise_time
		pose.airborne = true
		pose.feet = _rise_point(u)
		pose.rising = u < 0.5
		var stretch_u := clampf((m - takeoff) / 0.15, 0.0, 1.0) # a quick stretch off the ground
		pose.squash = 1.0 + stretch * sin(PI * stretch_u) - crouch_squash * (1.0 - _smooth(minf(stretch_u * 2.0, 1.0)))
		if m < swing_start:
			var w := _smooth(clampf((m - takeoff) / maxf((swing_start - takeoff) * 0.6, 1e-3), 0.0, 1.0))
			pose.phase = "rise"
			pose.angle = lerpf(deg_to_rad(wind_angle), cock, w)
			pose.lean = lerpf(0.25, -lean_back, w)
		else:
			var s := (m - swing_start) / swing
			var k := pow(s, 2.2) # slow start, violent end
			pose.phase = "smash"
			pose.angle = lerpf(cock, contact, k)
			pose.lean = lerpf(-lean_back, lean_smash, _smooth(s))
	elif m < landed: # recover: bounce off, hop back, land
		var u := (m - hit) / recover_time
		pose.phase = "hit" if t < hit + hitstop else "recover"
		pose.airborne = u < 1.0
		pose.feet = _hop_point(u)
		var kick := sin(PI * clampf(u / 0.35, 0.0, 1.0)) # the rebound, early in the recover
		pose.angle = contact + deg_to_rad(rebound) * kick
		pose.lean = lean_smash * (1.0 - 0.5 * _smooth(u))
	else: # settle: land, hammer back onto the shoulder
		var u := (m - landed) / settle_time
		pose.phase = "settle" if m < done else "done"
		pose.feet = landing()
		pose.angle = lerpf(contact, carry, _smooth(u))
		pose.lean = lean_smash * 0.5 * (1.0 - _smooth(u))
		var land := m - landed # the landing squash, springing back
		pose.squash = 1.0 - land_squash * exp(-land * 12.0) * cos(land * 20.0)
	return pose


# The jump: a cubic Bezier from the start up over the top to the contact spot.
func _rise_point(u: float) -> Vector3:
	var end := contact_feet()
	var top := maxf(_start.y, end.y) + jump_height
	var a := _start.lerp(end, 0.15)
	var b := _start.lerp(end, 0.75)
	a.y = _start.y + (top - _start.y) * 1.33
	b.y = end.y + (top - end.y) * 1.33
	return Bezier.point(_start, a, b, end, u)


# The hop back off the robot: up a little, down to the landing (gravity: slow, then faster).
func _hop_point(u: float) -> Vector3:
	var from := contact_feet()
	var to := landing()
	var a := from - _forward * land_back * 0.2 + Vector3.UP * bounce_height * 1.33
	var b := to + Vector3.UP * (bounce_height + (from.y - to.y) * 0.5)
	return Bezier.point(from, a, b, to, pow(u, 1.25))


static func _smooth(u: float) -> float:
	u = clampf(u, 0.0, 1.0)
	return u * u * (3.0 - 2.0 * u)
