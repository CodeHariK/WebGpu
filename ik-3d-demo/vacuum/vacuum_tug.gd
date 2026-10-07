## The capture mechanic — a ghost-vacuum tug-of-war — with no visuals of its own. It moves the
## hunter and the ghost and keeps the score; VacuumHunter, VacuumGhost and VacuumBeam draw it.
##
##   free      the ghost wanders the room (and edges away if you get close)
##   captured  hold suck with the ghost in front of the nozzle (CAPTURE_RANGE, CAPTURE_ANGLE).
##             The ghost flees, darting to a new direction every so often. The stream is a spring:
##             stretched past REST it pulls the two together (K), so the ghost drags you about and
##             you drag it back. Push the stick against the ghost's pull to fill `power`; its health
##             drains faster the fuller power is. Let go of suck, or let it get past
##             BREAK_DISTANCE, and it escapes.
##   slam      power full + slam: the ghost is swung over your head and smashed down on the other
##             side (SLAM_DAMAGE), then the tug goes on
##   suck_in   health gone: the ghost is reeled down the stream into the nozzle, shrinking
##   caught    gone; a new ghost appears after RESPAWN_TIME
class_name VacuumTug
extends RefCounted

const CAPTURE_RANGE := 5.0
const CAPTURE_ANGLE := deg_to_rad(28.0)
const REST := 2.2 ## metres of stream before it starts to pull
const K := 9.0 ## stream stiffness (m/s² per metre stretched)
const BREAK_DISTANCE := 6.5
const HOVER := 1.1 ## ghost height
const GHOST_PULL := 6.5 ## m/s² the ghost tugs with
const GHOST_DRAG := 1.6 ## ghost velocity damping (1/s)
const HUNTER_SHARE := 0.55 ## how much of the stream's pull reaches the hunter
const DRAIN := Vector2(4.0, 24.0) ## ghost health lost per second at power 0 … 1
const POWER_GAIN := 0.5 ## per second while pulling against the ghost
const POWER_LOSS := 0.25 ## per second otherwise
const AGAINST := 0.35 ## how directly the stick must oppose the ghost to count (dot product)
const SLAM_TIME := 0.55
const SLAM_DAMAGE := 22.0
const SLAM_HEIGHT := 2.6
const SUCK_TIME := 0.7
const RESPAWN_TIME := 1.6
const ARENA := 7.0 ## half size of the square room

var hunter: VacuumHunter
var ghost: VacuumGhost
var state := "free"
var power := 0.0 ## 0 … 1
var caught := 0
var shake := 0.0 ## camera shake, 0 … 1 (decays)
var flee := Vector3.FORWARD ## the ghost's current pull direction (flat)

var _state_time := 0.0
var _flee_left := 0.0
var _wander := Vector3.ZERO
var _slam_from := Vector3.ZERO
var _slam_to := Vector3.ZERO
var _suck_from := Vector3.ZERO


func _init(the_hunter: VacuumHunter, the_ghost: VacuumGhost) -> void:
	hunter = the_hunter
	ghost = the_ghost
	_wander = _random_spot()


## Advance one frame. `sucking` = suck held, `slam` = slam pressed this frame.
func update(delta: float, sucking: bool, slam: bool) -> void:
	_state_time += delta
	shake = move_toward(shake, 0.0, delta * 2.5)
	hunter.sucking = sucking and state != "caught"
	match state:
		"free": _free(delta, sucking)
		"captured": _captured(delta, sucking, slam)
		"slam": _slam(delta)
		"suck_in": _suck_in(delta)
		"caught": _caught(delta)
	_keep_inside(hunter)
	_keep_inside(ghost)


## Where the stream leaves the nozzle (its mouth), and which way.
func mouth() -> Vector3:
	return hunter.nozzle.global_transform * Vector3(0, 0, -0.26)


func mouth_direction() -> Vector3:
	return -hunter.nozzle.global_basis.z.normalized()


## True while the stream is on and attached to the ghost.
func streaming() -> bool:
	return state in ["captured", "slam", "suck_in"]


func _free(delta: float, sucking: bool) -> void:
	hunter.move(delta, Vector3.ZERO, 1.0)
	_face_movement()
	hunter.aim_target = Vector3.ZERO
	ghost.pull = Vector3.ZERO
	power = move_toward(power, 0.0, delta)
	var away := _flat(ghost.global_position - hunter.global_position)
	if ghost.global_position.distance_to(_wander) < 0.5 or _state_time > 4.0:
		_wander = _random_spot()
		_state_time = 0.0
	var want := _flat(_wander - ghost.global_position).normalized() * 1.2
	if away.length() < 2.5: # shy: drift away from the hunter
		want += away.normalized() * 1.5
	_fly_ghost(delta, (want - _flat(ghost.velocity)) * 2.0)
	if sucking and _in_cone():
		_enter("captured")
		power = 0.0
		_pick_flee()


func _captured(delta: float, sucking: bool, slam: bool) -> void:
	_flee_left -= delta
	if _flee_left <= 0.0:
		_pick_flee()
	var tension := _tension()
	ghost.pull = -tension.normalized() if tension != Vector3.ZERO else _flat(hunter.global_position - ghost.global_position).normalized()
	_fly_ghost(delta, flee * GHOST_PULL - tension - ghost.velocity * GHOST_DRAG)
	hunter.move(delta, _flat(tension) * HUNTER_SHARE, 0.6)
	_face_ghost()
	var against := hunter.input.length() > 0.3 and hunter.input.normalized().dot(-flee) > AGAINST
	power = clampf(power + (POWER_GAIN if against else -POWER_LOSS) * delta, 0.0, 1.0)
	ghost.health -= lerpf(DRAIN.x, DRAIN.y, power) * delta
	if ghost.health <= 0.0:
		ghost.health = 0.0
		_suck_from = ghost.global_position
		_enter("suck_in")
	elif not sucking or ghost.global_position.distance_to(hunter.global_position) > BREAK_DISTANCE:
		ghost.velocity += flee * 4.0 # it bolts
		ghost.pull = Vector3.ZERO
		power = 0.0
		_enter("free")
	elif slam and power >= 1.0:
		_slam_from = ghost.global_position
		var over := _flat(hunter.global_position - ghost.global_position).normalized()
		_slam_to = hunter.global_position + over * REST * 0.8
		_slam_to.y = 0.35
		_enter("slam")


# Over the hunter's head in an arc, smashed down on the far side.
func _slam(delta: float) -> void:
	var t := clampf(_state_time / SLAM_TIME, 0.0, 1.0)
	var eased := t * t * (3.0 - 2.0 * t)
	var position := _slam_from.lerp(_slam_to, eased)
	position.y += sin(t * PI) * SLAM_HEIGHT
	ghost.velocity = (position - ghost.global_position) / maxf(delta, 1e-4)
	ghost.global_position = position
	ghost.pull = _flat(hunter.global_position - ghost.global_position).normalized()
	hunter.move(delta, Vector3.ZERO, 0.2)
	_face_ghost()
	if t >= 1.0:
		ghost.health = maxf(ghost.health - SLAM_DAMAGE, 0.0)
		ghost.squash()
		ghost.velocity = Vector3.ZERO
		shake = 1.0
		power = 0.3
		if ghost.health <= 0.0:
			_suck_from = ghost.global_position
			_enter("suck_in")
		else:
			_pick_flee()
			_enter("captured")


# Reeled in along the stream, shrinking, into the nozzle.
func _suck_in(delta: float) -> void:
	var t := clampf(_state_time / SUCK_TIME, 0.0, 1.0)
	ghost.global_position = _suck_from.lerp(mouth(), t * t)
	ghost.shrink = 1.0 - t
	ghost.pull = (mouth() - ghost.global_position).normalized() if t < 1.0 else Vector3.ZERO
	hunter.move(delta, Vector3.ZERO, 0.3)
	_face_ghost()
	if t >= 1.0:
		caught += 1
		shake = 0.5
		_enter("caught")


func _caught(delta: float) -> void:
	hunter.move(delta, Vector3.ZERO, 1.0)
	_face_movement()
	hunter.aim_target = Vector3.ZERO
	ghost.pull = Vector3.ZERO
	power = 0.0
	if _state_time >= RESPAWN_TIME:
		respawn()


## A fresh ghost somewhere away from the hunter.
func respawn() -> void:
	var spot := _random_spot()
	for attempt in 8:
		if spot.distance_to(hunter.global_position) > 4.0:
			break
		spot = _random_spot()
	ghost.global_position = spot
	ghost.velocity = Vector3.ZERO
	ghost.health = VacuumGhost.MAX_HEALTH
	ghost.shrink = 1.0
	ghost.pull = Vector3.ZERO
	_enter("free")


# The stream's pull on the ghost (toward the hunter), zero while it's slack.
func _tension() -> Vector3:
	var to_ghost := ghost.global_position - mouth()
	var distance := to_ghost.length()
	if distance <= REST:
		return Vector3.ZERO
	return to_ghost / distance * K * (distance - REST)


# A new direction to bolt: away from the hunter, swerving well to one side.
func _pick_flee() -> void:
	var away := _flat(ghost.global_position - hunter.global_position)
	away = away.normalized() if away.length() > 0.01 else Vector3.FORWARD
	var swerve := randf_range(0.5, 1.7) * (1.0 if randf() < 0.5 else -1.0)
	flee = away.rotated(Vector3.UP, swerve)
	_flee_left = randf_range(0.7, 1.5)


func _in_cone() -> bool:
	var to_ghost := ghost.global_position - mouth()
	return to_ghost.length() < CAPTURE_RANGE and to_ghost.normalized().dot(mouth_direction()) > cos(CAPTURE_ANGLE)


func _fly_ghost(delta: float, acceleration: Vector3) -> void:
	ghost.velocity += acceleration * delta
	ghost.velocity.y = (HOVER - ghost.global_position.y) * 4.0 # float back to hover height
	ghost.global_position += ghost.velocity * delta


func _face_movement() -> void:
	var flat := _flat(hunter.velocity)
	if flat.length() > 0.3:
		hunter.facing = flat.normalized()


func _face_ghost() -> void:
	var to_ghost := _flat(ghost.global_position - hunter.global_position)
	if to_ghost.length() > 0.1:
		hunter.facing = to_ghost.normalized()
	hunter.aim_target = ghost.global_position


func _enter(new_state: String) -> void:
	state = new_state
	_state_time = 0.0


func _keep_inside(node: Node3D) -> void:
	var p := node.global_position
	node.global_position = Vector3(clampf(p.x, -ARENA, ARENA), p.y, clampf(p.z, -ARENA, ARENA))


static func _random_spot() -> Vector3:
	return Vector3(randf_range(-ARENA + 1.0, ARENA - 1.0), HOVER, randf_range(-ARENA + 1.0, ARENA - 1.0))


static func _flat(v: Vector3) -> Vector3:
	return Vector3(v.x, 0.0, v.z)
