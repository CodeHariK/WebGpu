## Personality through second-order springs (SecondOrder), configured by the spider's layout.
## Purely visual: the spider's real position, heading and gait never change; only how the body
## and head *show* them. Each part is off when its layout spring is zero, so for most layouts
## this node does nothing at all.
##
##   turn_spring  the body's heading follows the walking heading through a spring. r < 0: on a
##                new turn it first twists the wrong way (anticipation), then swings round, past,
##                and settles. → Spider.body_yaw_offset
##   move_spring  the body's position follows the walking position the same way: rocks back
##                before setting off, lurches forward on stopping. → Spider.body_offset
##   a detached head (layout.head) is four springs, each tuned on its own:
##     head_spring        position: chases its rest spot above the body (bobs, overshoots);
##                        a coil of rings is drawn from the neck to it if the layout wants one
##     head_yaw_spring    heading: the body's, or turned to watch look_target
##     head_pitch_spring  nod: looking up/down at look_target + tipping back while it lags
##     head_roll_spring   roll: swaying out as it swings sideways + banking into fast turns
##   (yaw/pitch/roll at zero use head_spring's numbers.) Pitch and roll *inputs* come from the
##   position spring's lag and the yaw spring's speed, so the springs drive each other a bit,
##   like real floppy parts.
##   hip_spring   each rigid leg's start point chases its hip spot on the body (SpiderRig
##                .rigid_hips), so the detached legs trail and settle after the body like the head.
class_name SpiderSprings
extends Node

const MAX_TWIST := 0.7 ## radians: never twist the body further than this from the heading
const MAX_SHIFT := 0.35 ## metres: never shift the body further than this
const TILT := 0.22 ## head tilt (radians) per m/s it lags behind the body
const TELEPORT := 1.5 ## a jump of more than this many metres in a frame resets the springs

const LOOK_RANGE := 1.8 ## radians: the head turns at most this far from the body to watch the target
const LOOK_PITCH := 0.45 ## radians: how far it tips up/down to watch it
const BANK := 0.04 ## head roll (radians) per rad/s the head is turning: leans into fast turns
const MAX_HIP_SHIFT := 0.2 ## metres: a sprung hip never strays further than this from the body

@export var spider: Spider
@export var look_target: Node3D ## the head turns to watch this (null = faces where the body faces)
@export var active := true:
	set(value):
		active = value
		if spider != null and not value:
			spider.body_yaw_offset = 0.0
			spider.body_offset = Vector3.ZERO
			if spider.rig != null:
				spider.rig.rigid_hips.clear()
		elif spider != null and spider.rig != null and is_inside_tree():
			_setup() # back on: start every spring at rest where things are now

var _turn: SecondOrder
var _move: SecondOrder
var _head: SecondOrder ## head position (x, y, z each spring the same way)
var _head_yaw: SecondOrder ## (single values are carried in .x)
var _head_pitch: SecondOrder
var _head_roll: SecondOrder
var _hips: Array = [] ## per leg: SecondOrder for a rigid leg's start point, or null
var _heading := 0.0 ## the spider's yaw, unwrapped (keeps counting past ±π)
var _last_yaw := 0.0
var _last_position := Vector3.ZERO


func _ready() -> void:
	spider.rebuilt.connect(_setup)
	_setup()


## (Re)create the springs from the current layout, at rest where the spider is now.
func _setup() -> void:
	var layout := spider.rig.layout
	_last_yaw = spider.global_rotation.y
	_heading = _last_yaw
	_last_position = spider.global_position
	_turn = _make(layout.turn_spring, Vector3(_heading, 0, 0))
	_move = _make(layout.move_spring, _flat(spider.global_position))
	_head = null
	_head_yaw = null
	_head_pitch = null
	_head_roll = null
	if spider.rig.head != null:
		_head = _make(layout.head_spring, _head_rest())
		_head_yaw = _make(_or(layout.head_yaw_spring, layout.head_spring), Vector3(_heading, 0, 0))
		_head_pitch = _make(_or(layout.head_pitch_spring, layout.head_spring), Vector3.ZERO)
		_head_roll = _make(_or(layout.head_roll_spring, layout.head_spring), Vector3.ZERO)
	_hips.clear()
	spider.rig.rigid_hips.clear()
	for leg in layout.legs.size():
		var rigid := spider.rig.rigid_limbs[leg] != null
		_hips.append(_make(layout.hip_spring, _hip_rest(leg)) if rigid else null)
		spider.rig.rigid_hips.append(null)
	spider.body_yaw_offset = 0.0
	spider.body_offset = Vector3.ZERO


func _physics_process(delta: float) -> void:
	if not active or spider == null or spider.rig == null:
		return
	if spider.global_position.distance_to(_last_position) > TELEPORT:
		_setup() # teleported (respawn): don't spring across the map
	_last_position = spider.global_position
	_heading += wrapf(spider.global_rotation.y - _last_yaw, -PI, PI)
	_last_yaw = spider.global_rotation.y
	_spring_turn(delta)
	_spring_move(delta)
	_spring_head(delta)
	_spring_hips(delta)


func _spring_turn(delta: float) -> void:
	if _turn == null:
		return
	var shown := _turn.update(delta, Vector3(_heading, 0, 0)).x
	spider.body_yaw_offset = clampf(shown - _heading, -MAX_TWIST, MAX_TWIST)


func _spring_move(delta: float) -> void:
	if _move == null:
		return
	var actual := _flat(spider.global_position)
	var shift := (_move.update(delta, actual) - actual).limit_length(MAX_SHIFT)
	spider.body_offset = _flat(spider.global_basis.inverse() * shift)


# Four springs. Position chases the rest spot above the (twisted, tilted) body. Yaw chases the
# body's heading — or, with a look_target, the direction to it (within LOOK_RANGE of the body),
# snapping round with whatever wind-up/wobble its spring has. Pitch chases the up/down look plus
# a tip back while the head lags behind the body; roll chases a sway out when it swings sideways
# plus a bank into fast turns — so a quick stop or snap-turn sets the head nodding and rocking.
func _spring_head(delta: float) -> void:
	if _head == null:
		return
	var position := _head.update(delta, _head_rest())
	var body_heading := _heading + spider.body_yaw_offset
	var look := Vector2.ZERO # (yaw from the body, pitch)
	if look_target != null:
		var to_target := look_target.global_position - position
		var flat := Vector2(to_target.x, to_target.z)
		if flat.length() > 0.05:
			look.x = clampf(wrapf(atan2(-to_target.x, -to_target.z) - body_heading, -PI, PI), -LOOK_RANGE, LOOK_RANGE)
			look.y = clampf(atan2(to_target.y, flat.length()), -LOOK_PITCH, LOOK_PITCH)
	var yaw := _head_yaw.update(delta, Vector3(body_heading + look.x, 0, 0)).x
	var facing := Basis.from_euler(Vector3(0, yaw, 0))
	var lag := facing.inverse() * (_head.velocity - spider.velocity) # m/s, in the head's frame
	var nod := look.y + lag.z * TILT # falling behind (+z) → face tips up
	var sway := -lag.x * TILT - _head_yaw.velocity.x * BANK # swinging right / turning left → rolls
	var pitch := clampf(_head_pitch.update(delta, Vector3(nod, 0, 0)).x, -0.8, 0.8)
	var roll := clampf(_head_roll.update(delta, Vector3(sway, 0, 0)).x, -0.6, 0.6)
	spider.rig.head.global_transform = Transform3D(Basis.from_euler(Vector3(pitch, yaw, roll)), position)
	if not spider.rig.coil.is_empty():
		_place_coil()


## Knock the head (recoil, a hit): `push` m/s added to its position spring, `nod` and `twist`
## rad/s to its pitch and yaw, `rock` rad/s to its roll. Its springs bring it back, wobbling.
func kick_head(push: Vector3, nod: float, twist := 0.0, rock := 0.0) -> void:
	if _head == null:
		return
	_head.kick(push)
	_head_pitch.kick(Vector3(nod, 0, 0))
	_head_yaw.kick(Vector3(twist, 0, 0))
	_head_roll.kick(Vector3(rock, 0, 0))


# Each rigid leg's start point chases its hip spot on the body (never more than MAX_HIP_SHIFT
# away), and SpiderRig hangs the leg from there.
func _spring_hips(delta: float) -> void:
	for leg in _hips.size():
		var spring: SecondOrder = _hips[leg]
		if spring == null:
			continue
		var rest := _hip_rest(leg)
		spider.rig.rigid_hips[leg] = rest + (spring.update(delta, rest) - rest).limit_length(MAX_HIP_SHIFT)


func _hip_rest(leg: int) -> Vector3:
	return spider.rig.skeleton.global_transform * spider.rig.layout.legs[leg].hip


# Rings along a curve from the neck (leaving the body straight up) to the underside of the head
# (arriving along the head's up): they bunch up and spread out as the head bobs — a spring.
func _place_coil() -> void:
	var body := spider.rig.skeleton.global_transform
	var head := spider.rig.head.global_transform
	var start: Vector3 = body * spider.rig.layout.head.neck
	var head_size: Vector3 = spider.rig.layout.head.size
	var finish := head.origin - head.basis.y * head_size.y * 0.5
	var reach := start.distance_to(finish) * 0.5
	var control_a := start + body.basis.y * reach
	var control_b := finish - head.basis.y * reach
	var rings := spider.rig.coil
	for i in rings.size():
		var t := (i + 0.5) / rings.size()
		var point := _bezier(start, control_a, control_b, finish, t)
		var tangent := (_bezier(start, control_a, control_b, finish, minf(t + 0.02, 1.0)) - _bezier(start, control_a, control_b, finish, maxf(t - 0.02, 0.0))).normalized()
		var helper := Vector3.FORWARD if absf(tangent.dot(Vector3.FORWARD)) < 0.9 else Vector3.RIGHT
		var x := tangent.cross(helper).normalized()
		rings[i].global_transform = Transform3D(Basis(x, tangent, x.cross(tangent)), point) # torus axis = Y


func _head_rest() -> Vector3:
	return spider.rig.skeleton.global_transform * spider.rig.layout.head.at


static func _bezier(a: Vector3, b: Vector3, c: Vector3, d: Vector3, t: float) -> Vector3:
	var u := 1.0 - t
	return a * u * u * u + b * 3.0 * u * u * t + c * 3.0 * u * t * t + d * t * t * t


## `spring` if it is set, else `fallback`.
static func _or(spring: Vector3, fallback: Vector3) -> Vector3:
	return spring if spring != Vector3.ZERO else fallback


static func _make(parameters: Vector3, start: Vector3) -> SecondOrder:
	if parameters == Vector3.ZERO:
		return null
	return SecondOrder.new(parameters.x, parameters.y, parameters.z, start)


static func _flat(v: Vector3) -> Vector3:
	return Vector3(v.x, 0.0, v.z)
