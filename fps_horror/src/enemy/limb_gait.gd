## How the ball moves: four limbs take turns stepping, and every step kicks the ball. Ported from
## LimbPlacementController (and Limb.InitializeStep) in the original.
##
##   step cycle  every step_interval one limb (left hand, right hand, left foot, right foot, round
##               and round) checks where it should be. If that spot is more than min_step from
##               where the limb is, it starts a step there (limb.gd: a Bezier arc) and kicks the
##               ball: feet push away from the new spot, hands pull toward it, both bent strongly
##               toward the body's desired_velocity; plus a little hop (step_bounce), minus the
##               ball's current velocity (so each kick steers rather than adds up)
##   limb spots  a ray from beside the shoulders (or hips), angled outward and down, `reach_down`
##               long; ahead of the ball by its velocity, so the limbs reach where it's going.
##               We also lead by desired_velocity: that's what gets a resting ball moving (in the
##               original, IK error made the limbs step now and then, which started it off).
##               And a ball that wants to move but is standing still steps anyway (otherwise
##               a ball pinned against something never steps again)
##   launch      when the body asks (onto/off a wall, toward the ceiling), all limbs plant, then
##               after 0.25 s one big impulse toward the next waypoint (and up)
##   in the air  limbs flail: each chases a spot circling round its usual one (sin/cos)
##   landing     every limb steps to the ground at once
##   torso       torso.gd: the body frame the cultist's chest is laid along
##   attack      attack(target): the same crouch-and-leap as a launch, aimed at the target,
##               flatter (half the height bonus); enemy_ai.gd decides when
##   stagger     stagger(s): no steps for a moment (hit by a shot), the ball just rolls
##   die         die(): limbs drop to the ground once and stay; no more steps or jumps
##
## show_limbs draws each limb point as a marker (step 6, before there was a body to see).
## Far-away enemies (step 11) can run only this file: the same movement, no IK.
##
## Mobile: one ray per step (about 8 a second); four while flying (rare, short).
extends Node3D

const EnemyBody := preload("res://src/enemy/enemy_body.gd")
const LimbPoint := preload("res://src/enemy/limb.gd")
const Torso := preload("res://src/enemy/torso.gd")

enum Limb { LEFT_HAND, RIGHT_HAND, LEFT_FOOT, RIGHT_FOOT }

@export var step_interval := 0.12  ## seconds between two limbs stepping
@export var min_step := 0.5  ## a limb steps once its spot is this far away (m)
@export var kick_speed := 8.0  ## impulse of a step along the move direction
@export var step_bounce := 5.0  ## upward part of each step's impulse
@export var lead := 0.45  ## how far ahead (s of velocity) limbs reach; also slows kicks when braking
@export var hand_bias := 0.8  ## how much hand kicks follow desired_velocity (vs. the limb's motion)
@export var foot_bias := 0.9
@export var launch_scale := 5.0
@export var max_launch := 40.0
@export var body_length := 1.0  ## hands this far ahead of feet
@export var shoulder_width := 0.5
@export var hip_width := 2.0  ## how far apart the limbs land
@export var reach_down := 2.5
@export var flail_speed := 15.0  ## radians per second
@export var flail_size := 3.0  ## metres
@export var show_limbs := true

var body: EnemyBody
var limbs: Array = []  ## LimbPoint × 4, in Limb order
var torso: Torso
var heading := Vector3.FORWARD  ## last direction the ball moved in
var launching := false
var alive := true

var _stagger := 0.0

var _timer := 0.0
var _next := 0
var _flight := 0.0
var _was_grounded := EnemyBody.Grounded.NONE
var _markers: Array[MeshInstance3D] = []


func _ready() -> void:
	body = get_node("../Body")
	body.launch_request.connect(_launch)
	torso = Torso.new(body.global_position)
	_timer = randf() * step_interval
	_next = randi() % 4
	for limb in 4:
		var point := LimbPoint.new(limb >= Limb.LEFT_FOOT, limb == Limb.LEFT_HAND or limb == Limb.LEFT_FOOT)
		var spot := limb_spot(limb)
		point.place(spot.position, spot.normal, spot.on_surface)
		limbs.append(point)
	if show_limbs:
		_make_markers()


func _physics_process(delta: float) -> void:
	if body.linear_velocity.length() > 0.5:
		heading = body.linear_velocity.normalized()
	var flying := body.grounded == EnemyBody.Grounded.NONE
	if body.grounded != _was_grounded:
		_flight = 0.0
		for point in limbs:
			point.set_airborne(flying and alive)
		if not flying:
			_step_all()
		_was_grounded = body.grounded
	_stagger = maxf(_stagger - delta, 0.0)
	if flying:
		_flight += delta * flail_speed
	elif not launching and alive and _stagger <= 0.0:
		_timer += delta
		while _timer >= step_interval:
			_timer -= step_interval
			_step(_next)
			_next = (_next + 1) % 4
	var stepping := 0
	for point in limbs:
		stepping += 1 if point.traveling else 0
	torso.update(delta, body.global_position, heading, _up(), stepping)
	for limb in 4:
		limbs[limb].update(delta, torso.frame, _flail_spot(limb) if flying and alive else Vector3.ZERO)
	_move_markers()


## One limb's turn: step if its spot moved far enough, kicking the ball.
func _step(limb: int) -> void:
	var point = limbs[limb]
	var spot := limb_spot(limb)
	var wants_to_go := body.desired_velocity != Vector3.ZERO
	var stuck := wants_to_go and body.linear_velocity.length() < 0.5  # pinned: step anyway
	if point.position.distance_to(spot.position) <= min_step and not stuck:
		return
	if (point.on_surface or spot.on_surface) and wants_to_go:
		var foot := limb >= Limb.LEFT_FOOT
		var motion: Vector3 = point.position - spot.position if foot else spot.position - point.position
		var direction := motion.normalized().lerp(body.desired_velocity, foot_bias if foot else hand_bias)
		_kick(direction.normalized(), point.position.lerp(body.global_position, 0.8))
	point.step_to(spot.position, spot.normal, spot.on_surface, body.global_position)


## Push the ball along `direction` from `at` (a point between the limb and the ball's centre, so
## steps also spin it a little).
func _kick(direction: Vector3, at: Vector3) -> void:
	var speed := kick_speed
	if body.linear_velocity.dot(body.desired_velocity) < 0.0:  # moving the wrong way: gentler
		speed *= body.desired_velocity.length() * lead
	var impulse := direction * speed + Vector3.UP * step_bounce - body.linear_velocity
	body.apply_impulse(impulse, at - body.global_position)


## The body wants to jump: leap at the next waypoint, high (twice the distance up).
func _launch() -> void:
	_leap_at(func() -> Vector3: return body.next_waypoint().position, 2.0)


## Lunge at `target` (enemy_ai.gd's attack). Returns false if already mid-jump.
func attack(target: Node3D) -> bool:
	if launching or not alive:
		return false
	_leap_at(func() -> Vector3: return target.global_position, 0.5)
	return true


func stagger(seconds: float) -> void:
	_stagger = maxf(_stagger, seconds)


func die() -> void:
	alive = false
	for point in limbs:
		point.set_airborne(false)
	_step_all()


## Plant everything, crouch for a moment, then one impulse toward where `aim_at` says (asked
## after the crouch, so it leads a moving target less), lifted by `lift` times the distance.
## Theirs: OnLaunchRequested (lift 2) and OnAttackLaunchRequested (lift 0.5).
func _leap_at(aim_at: Callable, lift: float) -> void:
	if launching or not alive:
		return
	launching = true
	_step_all()
	await get_tree().create_timer(0.25, false, true).timeout
	launching = false
	if not is_instance_valid(body) or not alive:
		return
	var to: Vector3 = aim_at.call()
	var aim := to + Vector3.UP * body.global_position.distance_to(to) * lift
	var impulse := (aim - body.global_position) * launch_scale - body.linear_velocity
	body.apply_impulse(impulse.limit_length(max_launch))


func _step_all() -> void:
	for limb in 4:
		var spot := limb_spot(limb)
		limbs[limb].step_to(spot.position, spot.normal, spot.on_surface, body.global_position)


class Spot:
	var position: Vector3
	var normal: Vector3
	var on_surface: bool


## Where `limb` wants to be: the ray hit, or the end of its reach if nothing is there.
func limb_spot(limb: int) -> Spot:
	var reach := _reach(limb, 1.0)
	return _cast(reach[0], reach[1])


## In the air: the usual spot pushed round a circle in the torso's side plane.
func _flail_spot(limb: int) -> Vector3:
	var reach := _reach(limb, 0.1 if limb >= Limb.LEFT_FOOT else 1.0)
	var angle: float = _flight + limbs[limb].fly_phase
	var flail := torso.frame.basis * (Vector3(0, sin(angle), cos(angle)) * flail_size)
	return _cast(reach[0], reach[1] + flail).position


## A `reach_down` ray from `from` toward `to`.
func _cast(from: Vector3, to: Vector3) -> Spot:
	var end := from + (to - from).normalized() * reach_down
	var query := PhysicsRayQueryParameters3D.create(from, end, body.world_mask, [body.get_rid()])
	var hit := body.get_world_3d().direct_space_state.intersect_ray(query)
	var spot := Spot.new()
	spot.on_surface = not hit.is_empty()
	spot.position = hit.position if spot.on_surface else to
	spot.normal = hit.normal if spot.on_surface else _up()
	return spot


## [ray start beside the shoulder or hip, the far point it aims at] for one limb.
## `velocity_lead`: how much of the ball's velocity to reach ahead by (feet barely, in the air).
func _reach(limb: int, velocity_lead: float) -> Array[Vector3]:
	var up := _up()
	var foot := limb >= Limb.LEFT_FOOT
	var left := limb == Limb.LEFT_HAND or limb == Limb.LEFT_FOOT
	var centre := body.global_position + up + (body.linear_velocity * velocity_lead + body.desired_velocity) * lead
	centre += heading * body_length * 0.5 * (-1.0 if foot else 1.0)
	var side := heading.cross(up).normalized() * (-1.0 if left else 1.0)
	return [centre + side * shoulder_width * 0.5, centre + side * hip_width * 0.5 - up * (reach_down + 1.0)]


## The surface normal, or UP; BACK when heading straight up or down on the floor (any up will do).
func _up() -> Vector3:
	var up := body.ground_normal
	if up == Vector3.UP and absf(heading.y) > 0.99:
		return Vector3.BACK
	return up


func _make_markers() -> void:
	var colors := [Color(0.9, 0.8, 0.7), Color(0.9, 0.8, 0.7), Color(0.8, 0.15, 0.1), Color(0.8, 0.15, 0.1)]
	for limb in 4:
		var mesh := SphereMesh.new()
		mesh.radius = 0.09
		mesh.height = 0.18
		mesh.radial_segments = 8
		mesh.rings = 4
		var mat := StandardMaterial3D.new()
		mat.shading_mode = BaseMaterial3D.SHADING_MODE_UNSHADED
		mat.albedo_color = colors[limb]
		mesh.material = mat
		var marker := MeshInstance3D.new()
		marker.mesh = mesh
		marker.top_level = true
		marker.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF
		add_child(marker)
		_markers.append(marker)


func _move_markers() -> void:
	for limb in _markers.size():
		_markers[limb].global_position = limbs[limb].position
