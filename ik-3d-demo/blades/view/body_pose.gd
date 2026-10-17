## What the blades ask of the player's body, shared by every body (capsule BladeBody, SophiaBody):
##   heading       which way the WHOLE body faces (legs too), sprung heavy: the mouse direction
##                 normally, but carried round by the blades in a spin or whirl
##   lean, twist   the torso on top of that, sprung (SecondOrder: overshoots a little and settles)
##   arm weight    per arm, 0 (the animation's arm) … 1 (reaching for its blade), eased
##   reach         per arm, where its hand wants to be, sprung so it lags a touch
##
## Heading:
##   spin    the body turns with the orbit, chest between the two blades (they're out to its sides)
##   whirl   the body faces the swung enemy (a hammer thrower)
##   yank    the body turns toward where the pull is throwing the enemy (left, right, behind:
##           it pivots with the throw; a throw straight behind always turns her to her right first,
##           so it never flips a coin between left and right)
##   else    the mouse direction (it springs back the short way round)
## The heading spring is slow and heavy, so the blades lead and the body is dragged after them;
## the twist then turns the chest a bit further toward where the blades already are.
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
##   rushing            forward (diving at the target)
class_name BodyPose
extends RefCounted

const MAX_LEAN := 0.6 ## radians-ish: how far the torso can tip
const MAX_TWIST := 1.2 ## radians: how far the chest can turn from the hips
const ARM_FADE := 0.15 ## seconds for an arm to blend between animation and IK
const LEAD := 0.5 ## share of the heading lag the chest makes up by twisting (chest ahead of the hips)

var lean := Vector3.ZERO ## horizontal: the direction the upper body tips; length = how far
var twist := 0.0
var heading := 0.0 ## the body's facing angle (unwrapped), atan2(forward.x, forward.z)
var arm_weight := PackedFloat32Array([0.0, 0.0]) ## [left, right]

var _demo: BladesDemo
var _spring := SecondOrder.new(2.5, 0.45, 1.2)
var _heading_spring := SecondOrder.new(1.6, 0.85, 0.0) ## heavy: no head start, little overshoot
var _heading_goal := 0.0 ## unwrapped target angle
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
	for i in 2:
		_reach[i].reset(hands[i])
		arm_weight[i] = 0.0


## Which way the body faces (horizontal unit vector).
func forward() -> Vector3:
	return Vector3(sin(heading), 0.0, cos(heading))


## Where the body is turning toward (the heading spring's goal), for the debug view.
func goal_forward() -> Vector3:
	return Vector3(sin(_heading_goal), 0.0, cos(_heading_goal))


## The body's right (its forward × up).
func right() -> Vector3:
	return forward().cross(Vector3.UP)


func update(delta: float) -> void:
	if delta <= 0.0: # hit-stop: hold the pose
		return
	# heading: follow the goal round, however many turns (unwrap by the short step each frame)
	_heading_goal += angle_difference(_heading_goal, _angle_of(_heading_target()))
	heading = _heading_spring.update(delta, Vector3(_heading_goal, 0.0, 0.0)).x
	var lag := _heading_goal - heading
	var pose := _spring.update(delta, _target())
	lean = Vector3(pose.x, 0.0, pose.z).limit_length(MAX_LEAN)
	twist = clampf(pose.y + lag * LEAD, -MAX_TWIST, MAX_TWIST)
	for i in 2:
		var wanted := 1.0 if is_reaching(i) else 0.0
		arm_weight[i] = move_toward(arm_weight[i], wanted, delta / ARM_FADE)


## Whether arm i (0 left, 1 right) is reaching for its blade right now.
func is_reaching(i: int) -> bool:
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
		var to_blade := _demo.blades[i].tip - shoulder
		wanted = shoulder + to_blade.normalized() * minf(to_blade.length(), arm_length * 0.95)
	return _reach[i].update(delta, wanted) if delta > 0.0 else _reach[i].value


# Where the whole body should face: carried round by a spin or a whirl, else the mouse.
func _heading_target() -> Vector3:
	var right_blade := _demo.blades[1]
	for blade in _demo.blades:
		if blade.phase == Blade.WHIRL: # face the swung enemy
			return _flat_toward(blade.tip, _demo.facing)
	if right_blade.phase == Blade.SPIN: # chest between the blades: the right blade at the right side
		return Vector3.UP.cross(_flat_toward(right_blade.tip, _demo.right()))
	for blade in _demo.blades:
		if blade.phase == Blade.YANK: # pivot with the throw
			return _throw_direction()
	return _demo.facing


# Where the pull is throwing the enemy (horizontal). Straight behind is nudged to her right, so
# the turn always goes the same way round.
func _throw_direction() -> Vector3:
	var fling: Dictionary = _demo.moves.pull.fling
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
	if _demo.moves.rush.rushing:
		tilt += front * 0.45
	for blade in _demo.blades:
		if blade.phase == Blade.IDLE or blade.delay > 0.0:
			continue
		var toward := _flat_toward(blade.tip, front)
		match blade.phase:
			Blade.OUT:
				if blade.segment_times.size() > 1 and blade.eased < 0.5: # winding up
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
				if blade.move == "pull" or blade.move == "rush":
					tilt -= front * 0.15
			Blade.SWEEP:
				tilt -= toward * 0.25
				turn += clampf(front.signed_angle_to(toward, Vector3.UP), -1.2, 1.2) * 0.4
			Blade.SPIN: # sit back into the turn (the two blades pull out evenly)
				tilt -= front * 0.12
			Blade.WHIRL: # counterweight: lean well away from the enemy (both blades add up)
				tilt -= toward * 0.22
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
