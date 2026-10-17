## Keeps a standing body's feet on the ground while it turns (a spin, a whirl, a pull's throw)
## instead of letting them skate round with the hips, and steps a foot when it gets left behind.
##
## Each foot is planted (it stays where it was put down, pointing the way it was put down) or
## stepping (it travels in a small arc to where the clip has it now: its "home"). A planted foot
## steps when its home has moved STEP_DISTANCE away or the body has turned STEP_TURN from the
## foot's heading. One foot steps at a time (the one furthest behind) — unless both are URGENT
## times past it, as in a fast spin, when the second one goes too.
##
## Fast turns: a step gets quicker the faster the body turns (down to STEP_TIME / FAST), and it
## lands a little ahead of its home (turned on round the body by turn rate × step time × AHEAD), so in a
## spin the feet keep up with the hips instead of trailing a step behind.
##
## `weight` fades planting in and out: running, jumping or rushing play the clip's feet (weight 0;
## the feet follow their homes), standing plants them (weight 1). It is the leg modifiers'
## influence.
class_name FootPlanter
extends RefCounted

const STEP_DISTANCE := 0.16 ## m: a planted foot steps once its home is this far away …
const STEP_TURN := 0.6 ## … or the body has turned this far (radians) from the foot's heading
const URGENT := 1.5 ## past this many times the thresholds a foot steps even while the other one is
const STEP_TIME := 0.12 ## seconds per step
const LIFT := 0.08 ## m: how high a step lifts the foot
const FADE := 0.15 ## seconds to blend planting in or out
const FAST := 2.5 ## a step is at most this many times quicker than STEP_TIME …
const FAST_TURN := 8.0 ## … reached at this turn rate (radians per second)
const AHEAD := 0.5 ## a fast-turn step lands this share of a step's turn ahead of its home


class Foot:
	var plant := Vector3.ZERO ## where it stands (world)
	var yaw := 0.0 ## its heading when put down (unwrapped, like BodyPose.heading)
	var step := -1.0 ## 0..1 while stepping, −1 planted
	var from := Vector3.ZERO ## where the step started
	var from_yaw := 0.0
	var duration := 0.12 ## this step's length (seconds)
	var target := Vector3.ZERO ## this frame's ankle target (world)
	var target_yaw := 0.0 ## this frame's foot heading


var feet: Array[Foot] = [Foot.new(), Foot.new()] ## [left, right]
var weight := 0.0
var turn_rate := 0.0 ## radians per second, signed, smoothed

var _last_heading := 0.0


func reset(
		homes: Array[Vector3],
		heading: float
) -> void:
	weight = 0.0
	turn_rate = 0.0
	_last_heading = heading
	for i in 2:
		_put_down(feet[i], homes[i], heading)


## `homes`: where the clip has each ankle now (world); `heading`: the body's heading;
## `planting`: whether the body is standing (feet planted) or not (the clip's feet).
func update(
		delta: float,
		homes: Array[Vector3],
		heading: float,
		planting: bool
) -> void:
	weight = move_toward(weight, 1.0 if planting else 0.0, delta / FADE)
	turn_rate = lerpf(turn_rate, (heading - _last_heading) / delta, 0.3)
	_last_heading = heading
	for i in 2:
		var foot := feet[i]
		if weight <= 0.0: # not planting: the feet just follow the clip
			_put_down(foot, homes[i], heading)
		elif foot.step >= 0.0:
			_carry(foot, delta, _ahead(homes, i), heading + turn_rate * foot.duration * AHEAD)
		else:
			foot.target = foot.plant
			foot.target_yaw = foot.yaw
	if weight > 0.0:
		_start_steps(homes, heading)


## How far behind foot i is (1 = at a step threshold): distance or turn, whichever is worse.
func behind(
		i: int,
		homes: Array[Vector3],
		heading: float
) -> float:
	var foot := feet[i]
	var flat := homes[i] - foot.plant
	flat.y = 0.0
	return maxf(flat.length() / STEP_DISTANCE, absf(heading - foot.yaw) / STEP_TURN)


# A stepping foot: along a small arc from where it lifted to its home (which may still be moving).
func _carry(
		foot: Foot,
		delta: float,
		home: Vector3,
		heading: float
) -> void:
	foot.step = minf(foot.step + delta / foot.duration, 1.0)
	var s := smoothstep(0.0, 1.0, foot.step)
	foot.target = foot.from.lerp(home, s) + Vector3.UP * sin(PI * foot.step) * LIFT
	foot.target_yaw = lerpf(foot.from_yaw, heading, s)
	if foot.step >= 1.0:
		_put_down(foot, home, heading)


# Lift the planted foot furthest behind (if past a threshold); the other one too if both are urgent.
func _start_steps(
		homes: Array[Vector3],
		heading: float
) -> void:
	var need := [behind(0, homes, heading), behind(1, homes, heading)]
	var first := 0 if need[0] >= need[1] else 1
	for i in [first, 1 - first]:
		var foot := feet[i]
		var other := feet[1 - i]
		if foot.step >= 0.0 or need[i] < 1.0:
			continue
		if other.step >= 0.0 and need[i] < URGENT:
			continue
		foot.step = 0.0
		foot.duration = STEP_TIME / lerpf(1.0, FAST, clampf(absf(turn_rate) / FAST_TURN, 0.0, 1.0))
		foot.from = foot.plant
		foot.from_yaw = foot.yaw


# Foot i's home a bit ahead: turned on round the body by turn rate × step time × AHEAD (so the foot
# lands leading the turn and stays planted longer).
func _ahead(
		homes: Array[Vector3],
		i: int
) -> Vector3:
	var center := (homes[0] + homes[1]) * 0.5
	center.y = homes[i].y
	return center + (homes[i] - center).rotated(Vector3.UP, turn_rate * feet[i].duration * AHEAD)


func _put_down(
		foot: Foot,
		where: Vector3,
		heading: float
) -> void:
	foot.plant = where
	foot.yaw = heading
	foot.step = -1.0
	foot.target = where
	foot.target_yaw = heading
