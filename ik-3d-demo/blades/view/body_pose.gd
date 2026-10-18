## What the blades ask of the player's body, shared by every body (capsule BladeBody, SophiaBody):
##   heading       which way the WHOLE body faces (legs too), sprung heavy: the mouse direction
##                 normally, but carried round by the blades in a spin or whirl
##   lean, twist   the torso on top of that, sprung (SecondOrder: overshoots a little and settles)
##   arm weight    per arm, 0 (the animation's arm) … 1 (reaching for its blade), eased
##   reach         per arm, where its hand wants to be, sprung (in the chest's frame) so it lags a
##                 touch; each arm stays
##                 in its own half (the left hand never crosses to the right of the chest)
##   look          where the head looks (sprung in the chest's frame): between the two blades while they're out, else
##                 the mouse direction
##
## Heading:
##   spin    the body turns with the orbit, chest between the two blades (they're out to its sides)
##   whirl   the body faces the swung enemy (a hammer thrower)
##   yank    the body turns toward where the pull (or slam) is throwing the enemy (left, right, behind:
##           it pivots with the throw; a throw straight behind always turns her to her right first,
##           so it never flips a coin between left and right)
##   else    the mouse direction (it springs back the short way round)
## The heading spring is slow and heavy, so the blades lead and the body is dragged after them;
## the twist then turns the chest a bit further toward where the blades already are. While the
## blades carry the body round (spin, whirl) the spring is aimed ahead of the goal by the goal's
## turn rate × the spring's own lag, so at a steady spin the body keeps up exactly (the right
## blade stays at her right) instead of trailing up to half a turn behind.
## The heading is an unwrapped angle (it can pass 2π), so a body spinning round and round never
## snaps back a full turn.
##
## How the torso leans (summed over both blades, then sprung):
##   wind-up            back, twisted toward the winding blade (anticipation)
##   strike / slash     forward, twisted the other way (follow-through)
##   yank, pull hold    back (bracing against the pull)
##   sweep              away from the blade, twisted toward it
##   spin               back and away from the leading blade (sitting into the turn)
##   whirl              well back, away from the swung enemy (counterweight)
##   rushing, hauled    forward (diving at the target / the ground)
##   rip tug            well back (heaving on the slab)
class_name BodyPose
extends RefCounted

const MAX_LEAN := 0.35 ## radians-ish: how far the torso can tip
const LEAN_SCALE := 0.5 ## every lean below is scaled by this (one knob for how much she leans)
const LOOK_PITCH := 0.5 ## the head follows this share of the blades' height (it doesn't stare up at them) …
const LOOK_UP := 0.2 ## … and looks at most this far up (radians) …
const LOOK_DOWN := 0.35 ## … or down
const MAX_TWIST := 1.2 ## radians: how far the chest can turn from the hips
const ARM_FADE := 0.15 ## seconds for an arm to blend between animation and IK
const LEAD := 0.5 ## share of the heading lag the chest makes up by twisting (chest ahead of the hips)
const HEADING_F := 1.6 ## heading spring: frequency (Hz) …
const HEADING_ZETA := 0.85 ## … and damping
const CARRY_LEAD := 2.0 * HEADING_ZETA / (TAU * HEADING_F) ## seconds: that spring's lag at a steady turn
const MAX_RATE := 25.0 ## rad/s: the goal's turn rate is clamped to this …
const JUMP := 0.6 ## … and a goal step bigger than this (radians) in one frame is a jump, not a turn
const MIN_OUT := 0.25 ## each arm points at least this much out to its own side (dot with chest right)
const EYE_HEIGHT := 1.55 ## m: where the look is measured from
const HEAVE := 0.45 ## the moment a yank starts, the hands snap in toward the body by this share …
const HEAVE_TIME := 0.3 ## … and out again over this long (the heave on the chain)
const YANK_KICK := 2.0 ## … and the torso is kicked back this hard (lean velocity)

var lean := Vector3.ZERO ## horizontal: the direction the upper body tips; length = how far
var twist := 0.0
var heading := 0.0 ## the body's facing angle (unwrapped), atan2(forward.x, forward.z)
var arm_weight := PackedFloat32Array([0.0, 0.0]) ## [left, right]

var _demo: BladesDemo
var _spring := SecondOrder.new(2.5, 0.45, 1.2)
var _heading_spring := SecondOrder.new(HEADING_F, HEADING_ZETA, 0.0) ## heavy: no head start, little overshoot
var _heading_goal := 0.0 ## unwrapped target angle
var _goal_rate := 0.0 ## rad/s, smoothed: how fast the goal is turning (only while carried)
var look := Vector3.FORWARD ## unit: where the head looks
var _look_spring := SecondOrder.new(3.0, 0.8, 0.5)
var _last_phase := [Blade.IDLE, Blade.IDLE] ## each blade's phase last frame (to see a yank start)
var _reach: Array[SecondOrder] = [SecondOrder.new(4.0, 0.6, 1.0), SecondOrder.new(4.0, 0.6, 1.0)]


func _init(demo: BladesDemo) -> void:
	_demo = demo


func reset(hands: Array[Vector3]) -> void:
	_spring.reset(Vector3.ZERO)
	lean = Vector3.ZERO
	twist = 0.0
	heading = _angle_of(_demo.facing)
	_heading_goal = heading
	_heading_spring.reset(Vector3(heading, 0.0, 0.0))
	_goal_rate = 0.0
	look = _demo.facing
	_look_spring.reset(look.rotated(Vector3.UP, -heading))
	for i in 2:
		_reach[i].reset((hands[i] - _demo.player).rotated(Vector3.UP, -heading))
		arm_weight[i] = 0.0


## Which way the body faces (horizontal unit vector).
func forward() -> Vector3:
	return Vector3(sin(heading), 0.0, cos(heading))


## Where the body is turning toward (the heading spring's goal), for the debug view.
func goal_forward() -> Vector3:
	return Vector3(sin(_heading_goal), 0.0, cos(_heading_goal))


## Which way the chest faces: the heading turned by the twist.
func chest_forward() -> Vector3:
	return forward().rotated(Vector3.UP, twist)


## The chest's right: the side the right arm keeps to.
func chest_right() -> Vector3:
	return chest_forward().cross(Vector3.UP)


## The body's right (its forward × up).
func right() -> Vector3:
	return forward().cross(Vector3.UP)


func update(delta: float) -> void:
	if delta <= 0.0: # hit-stop: hold the pose
		return
	# heading: follow the goal round, however many turns (unwrap by the short step each frame)
	var turn := angle_difference(_heading_goal, _angle_of(_heading_target()))
	_heading_goal += turn
	var rate := _goal_rate if absf(turn) > JUMP else clampf(turn / delta, -MAX_RATE, MAX_RATE)
	_goal_rate = lerpf(_goal_rate, rate if _carried() else 0.0, 1.0 - exp(-10.0 * delta))
	heading = _heading_spring.update(delta, Vector3(_heading_goal + _goal_rate * CARRY_LEAD, 0.0, 0.0)).x
	var lag := _heading_goal - heading
	_kick_on_yank()
	var pose := _spring.update(delta, _target())
	lean = Vector3(pose.x, 0.0, pose.z).limit_length(MAX_LEAN)
	twist = clampf(pose.y + lag * LEAD, -MAX_TWIST, MAX_TWIST)
	for i in 2:
		var wanted := 1.0 if is_reaching(i) else 0.0
		arm_weight[i] = move_toward(arm_weight[i], wanted, delta / ARM_FADE)
	var yaw := heading + twist # the look is sprung in the chest's frame: it turns with the body
	var looked := _look_spring.update(delta, _look_target().rotated(Vector3.UP, -yaw)).rotated(Vector3.UP, yaw)
	if looked.length() > 1e-3: # clamped again: a spring swinging the look far round can tip it up
		look = _level(looked.normalized(), 1.0)


## A shove to the torso (a gun's recoil rocks you back): the lean spring takes it.
func kick_back(push: Vector3) -> void:
	_spring.kick(push)


## Whether arm i (0 left, 1 right) is reaching for its blade right now (the right one always holds
## the revolver out, when it's the weapon).
func is_reaching(i: int) -> bool:
	if i == 1 and _demo.weapon == 2:
		return true
	var blade := _demo.blades[i]
	return blade.phase != Blade.IDLE and blade.delay <= 0.0


## Where arm i's hand should go: almost straight out toward its blade (or `rest`), sprung.
func reach(
		i: int,
		shoulder: Vector3,
		arm_length: float,
		rest: Vector3,
		delta: float
) -> Vector3:
	var wanted := rest
	if is_reaching(i):
		var blade := _demo.blades[i]
		var to_blade := blade.tip - shoulder
		var length := minf(to_blade.length(), arm_length * 0.95)
		if blade.phase == Blade.YANK: # the heave: hands in toward the body, then out again
			length *= 1.0 - HEAVE * sin(PI * clampf(blade.time / HEAVE_TIME, 0.0, 1.0))
		wanted = shoulder + _own_side(i, to_blade.normalized()) * length
	# sprung in the chest's frame, so a fast turn carries the hand round instead of leaving it behind
	var yaw := heading + twist
	var local := (wanted - _demo.player).rotated(Vector3.UP, -yaw)
	var sprung := _reach[i].update(delta, local) if delta > 0.0 else _reach[i].value
	var target := _demo.player + sprung.rotated(Vector3.UP, yaw)
	if is_reaching(i): # the spring's overshoot must not cross the chest either
		var to := target - shoulder
		target = shoulder + _own_side(i, to.normalized()) * to.length()
	return target


# A yank just started (either blade, once): kick the torso back, so the pull comes from the body.
func _kick_on_yank() -> void:
	var started := false
	for i in 2:
		var phase := _demo.blades[i].phase
		started = started or (phase == Blade.YANK and _last_phase[i] != Blade.YANK)
		_last_phase[i] = phase
	if started:
		var front := forward()
		_spring.kick(Vector3(-front.x, 0.0, -front.z) * YANK_KICK)


# `direction`, turned out to arm i's own side if it points across the chest: its part along the
# chest's right (signed by the side) is at least MIN_OUT. Straight across → straight out.
func _own_side(
		i: int,
		direction: Vector3
) -> Vector3:
	var out := chest_right() * (-1.0 if i == 0 else 1.0)
	var along := direction.dot(out)
	if along >= MIN_OUT:
		return direction
	var rest := direction - out * along
	if rest.length() < 1e-3:
		return out
	return out * MIN_OUT + rest.normalized() * sqrt(1.0 - MIN_OUT * MIN_OUT)


# Whether the blades are carrying the body round (spin, whirl).
func _carried() -> bool:
	for blade in _demo.blades:
		if blade.phase == Blade.SPIN or blade.phase == Blade.WHIRL:
			return true
	return false


# Where the head should look: between the two blades (the mean of the directions to them) while
# either is out, else the mouse direction. Blades straight opposite (a spin) → the chest's facing.
# Lasso → the mouse direction (where she'll throw), not the enemy over her head.
func _look_target() -> Vector3:
	for blade in _demo.blades:
		if blade.phase == Blade.LASSO: # swinging overhead: she watches where she'll throw
			return _demo.facing
	var eye := _demo.player + Vector3.UP * EYE_HEIGHT
	var sum := Vector3.ZERO
	var count := 0
	for i in 2:
		if is_reaching(i):
			sum += (_demo.blades[i].tip - eye).normalized()
			count += 1
	if count == 0:
		return _demo.facing
	if count == 2 and sum.length() < 0.5:
		return chest_forward()
	return _level(sum.normalized(), LOOK_PITCH)


# A look direction with its pitch scaled by `share` and clamped (LOOK_UP / LOOK_DOWN): the blades
# fly high (wind-up, slams), but she keeps her eyes near the horizon.
func _level(
		direction: Vector3,
		share: float
) -> Vector3:
	var flat := Vector3(direction.x, 0.0, direction.z)
	if flat.length() < 1e-3:
		flat = chest_forward()
	var pitch := clampf(asin(clampf(direction.y, -1.0, 1.0)) * share, -LOOK_DOWN, LOOK_UP)
	return flat.normalized() * cos(pitch) + Vector3.UP * sin(pitch)


# Where the whole body should face: carried round by a spin or a whirl, else the mouse.
func _heading_target() -> Vector3:
	var right_blade := _demo.blades[1]
	for blade in _demo.blades:
		if blade.phase == Blade.WHIRL: # face the swung enemy
			return _flat_toward(blade.tip, _demo.facing)
	if right_blade.phase == Blade.SPIN: # chest between the blades: the right blade at the right side
		return Vector3.UP.cross(_flat_toward(right_blade.tip, _demo.right()))
	for blade in _demo.blades:
		if blade.phase == Blade.YANK: # pivot with the throw (a pull's, or a slam's)
			return _throw_direction(blade)
	return _demo.facing


# Where the blade's pull (or slam) is throwing the enemy (horizontal). Straight behind is nudged
# to her right, so the turn always goes the same way round.
func _throw_direction(blade: Blade) -> Vector3:
	var move: BladeMove = _demo.moves[blade.move]
	var fling: Dictionary = (move as PullMove).fling if move is PullMove else {}
	var to: Vector3 = fling.get("to", _demo.facing)
	to.y = 0.0
	if to.length() < 0.5: # "at your feet": no turn
		return _demo.facing
	var direction := to.normalized()
	if direction.dot(_demo.facing) < -0.9:
		direction = direction.rotated(Vector3.UP, -0.3) # a little toward her right
	return direction


# What the blades ask of the torso this frame: (lean x, twist, lean z), relative to the body.
func _target() -> Vector3:
	var front := forward()
	var tilt := Vector3.ZERO
	var turn := 0.0
	if _demo.moves.rip.tug_time > 0.0: # heaving on the slab: lean right back
		tilt -= front * 0.7
	if _demo.moves.rush.rushing or _demo.moves.anchor.hauling:
		tilt += front * 0.45
	for blade in _demo.blades:
		if blade.phase == Blade.IDLE or blade.delay > 0.0:
			continue
		var toward := _flat_toward(blade.tip, front)
		match blade.phase:
			Blade.OUT:
				if blade.eased < blade.swing_end: # winding up
					tilt -= front * 0.3
					turn += blade.side * 0.4
				else:
					tilt += front * 0.3
					turn -= blade.side * 0.3
			Blade.SLICE:
				tilt += front * 0.25
				turn -= blade.side * 0.3
			Blade.YANK:
				tilt -= front * 0.35
			Blade.HOLD:
				if blade.move == "pull" or blade.move == "rush" or blade.move == "rip":
					tilt -= front * 0.15
			Blade.SWEEP:
				tilt -= toward * 0.25
				turn += clampf(front.signed_angle_to(toward, Vector3.UP), -1.2, 1.2) * 0.4
			Blade.SPIN: # sit back into the turn (the two blades pull out evenly)
				tilt -= front * 0.12
			Blade.LASSO: # a little away from the swung enemy
				tilt -= toward * 0.12
			Blade.WHIRL: # counterweight: lean well away from the enemy (both blades add up)
				tilt -= toward * 0.22
	tilt *= LEAN_SCALE
	return Vector3(tilt.x, turn, tilt.z)


# Horizontal direction from the player to `point` (or `fallback` when it's right on top).
func _flat_toward(
		point: Vector3,
		fallback: Vector3
) -> Vector3:
	var to := point - _demo.player
	to.y = 0.0
	return to.normalized() if to.length() > 0.2 else fallback


static func _angle_of(direction: Vector3) -> float:
	return atan2(direction.x, direction.z)
