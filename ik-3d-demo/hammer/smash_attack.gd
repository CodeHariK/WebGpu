## KingClown's hammer attacks, as a pure function of time: sample(t) says where his feet are, which
## way the hammer points, where his hands are, how much he leans, twists and squashes at any moment
## t of the attack — so the demo can play it, pause it, scrub it and replay it exactly.
##
## One attack, from a snapshot taken when it starts:
##   crouch   he sinks (squash), the hammer drops low behind him, hands down by his hip
##   rise     he jumps on a Bezier arc; the hammer cocks behind his head with his hands up above it,
##            his chest turns away (twist) and he arches back
##   smash    the last `swing_time` of the rise: the hammer whips over the top and down onto the
##            target (fast at the end), his chest turns back through, he folds forward
##   hit      `hitstop` seconds frozen at the contact
##   recover  the hammer bounces back off, he hops back and lands (squash)
##   settle   the hammer comes back down to rest on the ground in front of him (the idle pose)
##
## Three kinds (the snapshot):
##   aim()         a jump smash onto a robot's head
##   aim_ground()  a ground pound: he lands and slams the ground in front of him (the shockwave)
##   chain()       a head-hop: from where he is in the air (bouncing off a head) straight into the
##                 next jump smash — no crouch, the hammer and body carry on from where they were
##
## The swing is two levers, like a real one: his HANDS move on a short arc round his shoulders
## (hand_* angles: down by the hip, up above the head, out in front) and the HAMMER points from
## the hands along its own angle. So with the hammer cocked behind his head his hands stay up above
## it — the arms never go back through his body. Angles are in his swing plane (forward / up):
## 0° straight ahead, 90° straight up, 180° straight behind; the plane sits `side` to his right.
##
## Where he must be at the contact is worked out backwards from the target: hands and hammer at
## their contact angles put the hammer's head on it, which fixes his feet — the jump ends there.
class_name SmashAttack
extends RefCounted

const GROUND := 0.0 ## the ground's height

## Everything sample() returns for one moment.
class Pose:
	var feet := Vector3.ZERO
	var forward := Vector3.FORWARD
	var angle := 0.0 ## the hammer's angle (radians, see the header)
	var hand_angle := 0.0 ## the hands' angle round his shoulders (radians)
	var lean := 0.0 ## radians the upper body tips forward (negative: back)
	var twist := 0.0 ## radians the chest turns (positive: to his left, toward the swing's follow-through)
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
var reach := 2.4 ## m from his shoulders to the hammer's head (hands out at hand_radius, then the handle)
var head_radius := 0.45 ## the hammer's barrel: radius …
var head_length := 0.85 ## … and length
var handle_radius := 0.065
var handle_end := 0.15 ## m the handle sticks out past the right hand
var grip_gap := 0.32 ## m the left hand grips up the handle from the right
var hand_radius := 0.7 ## m from his shoulders to his (right) hand
var side := 0.3 ## the swing plane, this far to his right
var carry_angle := -20.0 ## degrees: standing, its head resting on the ground in front of him
var wind_angle := 205.0 ## crouch: low behind
var cock_angle := 165.0 ## rising: up behind his head
var contact_angle := -35.0 ## the smash lands with the hammer this far below level
var ground_angle := -70.0 ## a ground pound's hammer angle — worked out each time so its face just reaches the ground; this is only used if it can't
var rebound := 30.0 ## degrees the hammer bounces back up off what it hit
var hand_carry := -40.0 ## the hands' angle: low in front of his belly, standing
var hand_wind := -85.0 ## … down by his hip, winding up
var hand_cock := 100.0 ## … up above his head, cocked
var hand_contact := -15.0 ## … out in front, at the contact
var hand_ground := -65.0 ## … down low in front, slamming the ground
var lean_back := 0.35 ## radians he arches back while rising
var lean_smash := 0.6 ## radians he folds forward into the smash
var twist_cock := -0.45 ## radians his chest turns away (to the hammer's side) while cocked
var twist_smash := 0.35 ## … and through, at the contact
var crouch_squash := 0.22
var stretch := 0.15 ## takeoff stretch
var land_squash := 0.2
var bounce_height := 0.5 ## the hop back off the hit
var land_back := 0.7 ## he lands this far back from where he hit (m)

var shoulder_height := 1.3 ## his shoulders above his feet (the body measures it)

var _start := Vector3.ZERO
var _forward := Vector3.FORWARD
var _head := Vector3.ZERO ## the point the hammer's head lands on (its surface)
var _ground := false ## a ground pound
var _chained: Pose = null ## a head-hop: the pose it carries on from (null: a fresh attack)
var _ground_deg := 0.0 ## a ground pound: the hammer angle that puts its face on the ground


## The tuning sliders: [var name, min, max, step]; a String starts a new tab of the panel.
func tunables() -> Array:
	return [
		"Hammer",
		["reach", 1.0, 3.5, 0.05], ["head_radius", 0.15, 0.9, 0.01], ["head_length", 0.3, 1.6, 0.01],
		["handle_radius", 0.02, 0.15, 0.005], ["handle_end", 0.0, 0.6, 0.01], ["grip_gap", 0.1, 0.8, 0.01],
		"Idle pose",
		["hand_radius", 0.2, 1.2, 0.01], ["side", 0.0, 0.9, 0.01], ["carry_angle", -90.0, 200.0, 1.0], ["hand_carry", -110.0, 60.0, 1.0],
		"Swing",
		["wind_angle", 150.0, 260.0, 1.0], ["cock_angle", 100.0, 220.0, 1.0], ["contact_angle", -80.0, 20.0, 1.0], ["rebound", 0.0, 90.0, 1.0],
		["hand_wind", -120.0, 0.0, 1.0], ["hand_cock", 40.0, 140.0, 1.0], ["hand_contact", -80.0, 40.0, 1.0], ["hand_ground", -110.0, 0.0, 1.0],
		["lean_back", 0.0, 0.8, 0.01], ["lean_smash", 0.0, 1.0, 0.01], ["twist_cock", -1.0, 0.5, 0.01], ["twist_smash", -0.5, 1.0, 0.01],
		"Jump & timing",
		["crouch_time", 0.05, 0.6, 0.01], ["rise_time", 0.2, 1.2, 0.01], ["swing_time", 0.05, 0.5, 0.01],
		["hitstop", 0.0, 0.4, 0.01], ["recover_time", 0.15, 1.0, 0.01], ["settle_time", 0.1, 1.0, 0.01],
		["jump_height", 0.3, 4.0, 0.05], ["crouch_squash", 0.0, 0.5, 0.01], ["stretch", 0.0, 0.5, 0.01], ["land_squash", 0.0, 0.5, 0.01],
		["bounce_height", 0.0, 2.0, 0.05], ["land_back", 0.0, 2.0, 0.05],
	]


## A jump smash from `feet` onto the top of a robot's head.
func aim(
		feet: Vector3,
		head_top: Vector3
) -> void:
	_snapshot(feet, head_top, false, null)


## A ground pound from `feet`, slamming the ground at `spot`.
func aim_ground(
		feet: Vector3,
		spot: Vector3
) -> void:
	_snapshot(feet, Vector3(spot.x, GROUND, spot.z), true, null)


## A head-hop: carry on from `from` (his pose now, in the air) into a jump smash on the next head.
func chain(
		from: Pose,
		head_top: Vector3
) -> void:
	_snapshot(from.feet, head_top, false, from)


## A ground pound (the demo sends out the shockwave)?
func is_ground() -> bool:
	return _ground


## Seconds from the start to the contact, and to the end.
func contact_time() -> float:
	return _crouch() + rise_time


func length() -> float:
	return contact_time() + hitstop + recover_time + settle_time


## Where his feet are at the contact (worked back from the target).
func contact_feet() -> Vector3:
	var feet := _head - _contact_offset()
	if _ground: # his feet are on the ground: the hammer lands where it reaches from there
		feet.y = GROUND
	return feet


## Where the hammer's head lands (its surface touching the target).
func contact_point() -> Vector3:
	if _ground:
		return contact_feet() + _contact_offset()
	return _head


## Where he lands after the hop back.
func landing() -> Vector3:
	var at := contact_feet() - _forward * land_back
	return Vector3(at.x, GROUND, at.z)


## A standing pose at `feet` facing `forward`: hammer on the shoulder (walking about between attacks).
func stand(
		feet: Vector3,
		forward: Vector3
) -> Pose:
	var pose := Pose.new()
	pose.feet = feet
	pose.forward = forward
	pose.angle = deg_to_rad(carry_angle)
	pose.hand_angle = deg_to_rad(hand_carry)
	pose.phase = "free"
	return pose


## Between his shoulders at `pose` (the swing's centre; it squashes with him).
func shoulders(pose: Pose) -> Vector3:
	return pose.feet + Vector3.UP * shoulder_height * pose.squash


## His right hand (the handle's end) at `pose`.
func hands(pose: Pose) -> Vector3:
	return shoulders(pose) + right_of(pose.forward) * side + direction(pose.forward, pose.hand_angle) * hand_radius


## The point `distance` along the hammer at `pose`, measured as if from his shoulders: hand_radius
## is his right hand, reach the hammer head's centre.
func hammer_point(
		pose: Pose,
		distance: float
) -> Vector3:
	return hands(pose) + direction(pose.forward, pose.angle) * (distance - hand_radius)


## The hammer's (or the hands') direction from its pivot at angle θ in the swing plane of `forward`.
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


## Everything at time t (seconds from the start; clamped to the attack).
func sample(t: float) -> Pose:
	var pose := Pose.new()
	pose.forward = _forward
	t = clampf(t, 0.0, length())
	var hit := contact_time()
	pose.since_hit = t - hit if t >= hit else -1.0
	var m := t if t < hit else (hit if t < hit + hitstop else t - hitstop) # motion time: frozen in the hit-stop
	var takeoff := _crouch()
	var swing := minf(swing_time, rise_time * 0.9)
	var swing_start := hit - swing
	var landed := hit + recover_time
	var done := landed + settle_time
	var contact := deg_to_rad(_contact_angle())
	var wound := _wound_up()
	if m < takeoff: # crouch
		var u := _smooth(m / takeoff)
		pose.phase = "crouch"
		pose.feet = _start
		pose.angle = lerpf(deg_to_rad(carry_angle), wound.angle, u)
		pose.hand_angle = lerpf(deg_to_rad(hand_carry), wound.hand_angle, u)
		pose.lean = wound.lean * u
		pose.twist = wound.twist * u
		pose.squash = 1.0 - crouch_squash * u
	elif m < hit: # rise (and the smash at its end)
		var u := (m - takeoff) / rise_time
		pose.airborne = true
		pose.feet = _rise_point(u)
		pose.rising = u < 0.5
		var stretch_u := clampf((m - takeoff) / 0.15, 0.0, 1.0) # a quick stretch off the ground
		var from_squash := wound.squash
		pose.squash = 1.0 + stretch * sin(PI * stretch_u) * (0.0 if _chained != null else 1.0) - (1.0 - from_squash) * (1.0 - _smooth(minf(stretch_u * 2.0, 1.0)))
		if m < swing_start: # cock: hammer behind his head, hands up, chest turned away, arched
			var w := _smooth(clampf((m - takeoff) / maxf((swing_start - takeoff) * 0.6, 1e-3), 0.0, 1.0))
			pose.phase = "rise"
			pose.angle = lerpf(wound.angle, deg_to_rad(cock_angle), w)
			pose.hand_angle = lerpf(wound.hand_angle, deg_to_rad(hand_cock), w)
			pose.lean = lerpf(wound.lean, -lean_back, w)
			pose.twist = lerpf(wound.twist, twist_cock, w)
		else: # smash: over the top, hands down to the front, chest through, folding forward
			var s := (m - swing_start) / swing
			var k := pow(s, 2.2) # slow start, violent end
			pose.phase = "smash"
			pose.angle = lerpf(deg_to_rad(cock_angle), contact, k)
			pose.hand_angle = lerpf(deg_to_rad(hand_cock), deg_to_rad(_contact_hand()), _smooth(s))
			pose.lean = lerpf(-lean_back, lean_smash, _smooth(s))
			pose.twist = lerpf(twist_cock, twist_smash, _smooth(s))
	elif m < landed: # recover: bounce off, hop back, land
		var u := (m - hit) / recover_time
		pose.phase = "hit" if t < hit + hitstop else "recover"
		pose.airborne = u < 1.0 and not _ground
		pose.feet = _hop_point(u)
		var kick := sin(PI * clampf(u / 0.35, 0.0, 1.0)) # the rebound, early in the recover
		pose.angle = contact + deg_to_rad(rebound) * kick
		pose.hand_angle = deg_to_rad(_contact_hand()) + deg_to_rad(rebound) * 0.4 * kick
		pose.lean = lean_smash * (1.0 - 0.5 * _smooth(u))
		pose.twist = twist_smash * (1.0 - 0.5 * _smooth(u))
		if _ground: # he slammed from the ground: the landing squash is the slam itself
			pose.squash = 1.0 - land_squash * exp(-(m - hit) * 10.0) * cos((m - hit) * 18.0)
	else: # settle: land, hammer back onto the shoulder
		var u := _smooth((m - landed) / settle_time)
		pose.phase = "settle" if m < done else "done"
		pose.feet = landing()
		pose.angle = lerpf(contact, deg_to_rad(carry_angle), u)
		pose.hand_angle = lerpf(deg_to_rad(_contact_hand()), deg_to_rad(hand_carry), u)
		pose.lean = lean_smash * 0.5 * (1.0 - u)
		pose.twist = twist_smash * 0.5 * (1.0 - u)
		var land := m - landed # the landing squash, springing back
		pose.squash = 1.0 - land_squash * exp(-land * 12.0) * cos(land * 20.0)
	return pose


func _snapshot(
		feet: Vector3,
		target: Vector3,
		ground: bool,
		chained: Pose
) -> void:
	_start = feet
	_head = target
	_ground = ground
	_chained = chained
	var flat := Vector3(target.x - feet.x, 0.0, target.z - feet.z)
	_forward = flat.normalized() if flat.length() > 0.05 else (chained.forward if chained != null else Vector3.FORWARD)
	if ground:
		_ground_deg = _solve_ground_angle()


# A head-hop skips the crouch (it takes off from the bounce).
func _crouch() -> float:
	return 0.0 if _chained != null else crouch_time


func _contact_hand() -> float:
	return hand_ground if _ground else hand_contact


func _contact_angle() -> float:
	return _ground_deg if _ground else contact_angle


# The ground pound's hammer angle: the one (between −95° and −5°) whose face just touches the
# ground with his feet on it — bisection on the face's height, which rises with the angle. If even
# the steepest can't reach, ground_angle.
func _solve_ground_angle() -> float:
	var low := -95.0
	var high := -5.0
	_ground_deg = low
	if _contact_offset().y > GROUND:
		return ground_angle
	for i in 30:
		_ground_deg = (low + high) * 0.5
		if _contact_offset().y < GROUND:
			low = _ground_deg
		else:
			high = _ground_deg
	return _ground_deg


# Where the takeoff starts from: the crouch's end (a fresh attack) or the pose it carries on from.
func _wound_up() -> Pose:
	if _chained != null:
		return _chained
	var pose := Pose.new()
	pose.angle = deg_to_rad(wind_angle)
	pose.hand_angle = deg_to_rad(hand_wind)
	pose.lean = 0.25
	pose.twist = twist_cock * 0.5
	pose.squash = 1.0 - crouch_squash
	return pose


# The hammer head's centre relative to his feet at the contact (shoulders, hands out, handle on),
# minus the head's radius so its face — not its centre — touches the target.
func _contact_offset() -> Vector3:
	var hand := deg_to_rad(_contact_hand())
	var angle := deg_to_rad(_contact_angle())
	var head := Vector3.UP * shoulder_height + right_of(_forward) * side + direction(_forward, hand) * hand_radius + direction(_forward, angle) * (reach - hand_radius)
	return head - swing_of(_forward, angle) * head_radius # the leading face: one radius on from the centre (θ falls: it moves along −swing)


# The jump: a cubic Bezier from the start up over the top to the contact spot.
func _rise_point(u: float) -> Vector3:
	var end := contact_feet()
	var top := maxf(_start.y, end.y) + jump_height
	if _ground:
		top = _start.y + jump_height
	var a := _start.lerp(end, 0.15)
	var b := _start.lerp(end, 0.75)
	a.y = _start.y + (top - _start.y) * 1.33
	b.y = end.y + (top - end.y) * 1.33
	return Bezier.point(_start, a, b, end, u)


# The hop back off the target: up a little, down to the landing (gravity: slow, then faster).
func _hop_point(u: float) -> Vector3:
	var from := contact_feet()
	var to := landing()
	if _ground: # already on the ground: a small step back
		return from.lerp(to, _smooth(u))
	var a := from - _forward * land_back * 0.2 + Vector3.UP * bounce_height * 1.33
	var b := to + Vector3.UP * (bounce_height + (from.y - to.y) * 0.5)
	return Bezier.point(from, a, b, to, pow(u, 1.25))


static func _smooth(u: float) -> float:
	u = clampf(u, 0.0, 1.0)
	return u * u * (3.0 - 2.0 * u)
