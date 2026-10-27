## The revolver's rules: six rounds, a fire cooldown, spread, hitscan, reload. It asks the arms
## animator for permission (no firing mid-reload), then fires a ray from the camera through the
## centre of the screen (plus spread), so the bullet always goes where the crosshair says.
##
##   spread   small when aiming, wide from the hip; every shot adds bloom that fades over time
##   recoil   kicks the view up a little (that stays, like the original) and shakes the camera
##   reload   R, or automatically when you pull the trigger on an empty gun; the rounds are
##            refilled when the reload clip finishes
##   hits     RigidBodies get pushed; the `hit` signal tells anyone else (impacts now, enemy
##            damage in step 8)
##
## Mobile: one ray query per shot, nothing per frame except two timers.
extends Node

const Animator := preload("res://src/player/arms_animator.gd")

signal fired(origin: Vector3, direction: Vector3)
signal hit(collider: Object, point: Vector3, normal: Vector3, direction: Vector3)
signal ammo_changed(rounds: int)
signal reload_started(spent: int)

@export var capacity := 6
@export var cooldown := 0.38  ## seconds between shots (about the fire clip)
@export var max_range := 120.0
@export var spread_aimed := deg_to_rad(0.4)
@export var spread_hip := deg_to_rad(2.5)
@export var bloom_per_shot := deg_to_rad(1.2)
@export var bloom_decay := deg_to_rad(3.0)  ## per second
@export var kick_aimed := deg_to_rad(1.0)
@export var kick_hip := deg_to_rad(1.8)
@export var push := 6.0  ## impulse on RigidBodies (N·s)

var rounds := 6
var reloading := false

var _animator: Animator
var _camera: Camera3D
var _body: CollisionObject3D
var _cooldown_left := 0.0
var _bloom := 0.0
var _reload_seen := false  # the reload clip has started (travel() takes a frame or two)


func setup(animator: Animator, camera: Camera3D, body: CollisionObject3D) -> void:
	_animator = animator
	_camera = camera
	_body = body
	rounds = capacity


func _process(delta: float) -> void:
	_cooldown_left = maxf(_cooldown_left - delta, 0.0)
	_bloom = maxf(_bloom - bloom_decay * delta, 0.0)
	if reloading:
		var in_clip := _animator.current() in [&"LowReload", &"HighReload"]
		_reload_seen = _reload_seen or in_clip
		if _reload_seen and not in_clip:
			reloading = false
			rounds = capacity
			ammo_changed.emit(rounds)


func aiming() -> bool:
	return _animator.current() in [&"HighIdle", &"HighFire"]


## Pull the trigger. Returns true if a shot went off.
func try_fire() -> bool:
	if _cooldown_left > 0.0 or reloading:
		return false
	if rounds <= 0:
		try_reload()
		return false
	var aimed := aiming()
	if not _animator.fire():
		return false
	rounds -= 1
	_cooldown_left = cooldown
	ammo_changed.emit(rounds)
	var direction := _spread_direction(spread_aimed if aimed else spread_hip)
	_bloom += bloom_per_shot
	var origin := _camera.global_position
	fired.emit(origin, direction)
	_kick(kick_aimed if aimed else kick_hip)
	_cast(origin, direction)
	return true


func try_reload() -> bool:
	if reloading or rounds == capacity:
		return false
	if _animator.reload():
		reloading = true
		_reload_seen = false
		reload_started.emit(capacity - rounds)
		return true
	return false


## The view direction, tilted by a random angle inside the spread cone.
func _spread_direction(base: float) -> Vector3:
	var cone := base + _bloom
	var forward := -_camera.global_basis.z
	var angle := randf() * TAU
	var off := sqrt(randf()) * cone  # uniform over the cone's disc
	var tilt := _camera.global_basis.x * cos(angle) + _camera.global_basis.y * sin(angle)
	return forward.rotated(tilt.cross(forward).normalized(), off).normalized() if off > 0.0 else forward


func _cast(origin: Vector3, direction: Vector3) -> void:
	var query := PhysicsRayQueryParameters3D.create(origin, origin + direction * max_range)
	query.exclude = [_body.get_rid()]
	var result := _camera.get_world_3d().direct_space_state.intersect_ray(query)
	if result.is_empty():
		return
	var collider: Object = result.collider
	if collider is RigidBody3D:
		collider.apply_impulse(direction * push, result.position - collider.global_position)
	hit.emit(collider, result.position, result.normal, direction)


func _kick(pitch: float) -> void:
	var head := _camera.get_parent()
	if head.has_method("recoil"):
		head.recoil(pitch * randf_range(0.8, 1.2), pitch * randf_range(-0.3, 0.3))
	if _camera.has_method("shake"):
		_camera.shake(0.6)
