## The player's body: a CharacterBody3D capsule with its origin at the feet.
## Walk, sprint (forward only), crouch (hold; won't stand up under a low ceiling), jump, and step up
## small ledges like stair steps and debris without jumping.
##
## Mobile notes: one capsule, one move_and_slide per physics tick; step-up adds a few test_move
## calls only when something is actually in the way.
extends CharacterBody3D

signal landed(fall_speed: float)  ## hit the ground after falling (m/s, positive)
signal stepped(height: float)  ## popped up a step this tick (the camera smooths it out)
signal hit_taken(point: Vector3, force: Vector3, attacker: Node3D)  ## see take_hit()

@export var walk_speed := 4.0
@export var sprint_speed := 6.5
@export var crouch_speed := 2.0
@export var ground_accel := 12.0  ## how fast velocity catches up with the input on the ground
@export var air_accel := 2.5
@export var jump_velocity := 5.0
@export var gravity := 14.0
@export var step_height := 0.35
@export var step_probe := 0.12  ## how far ahead to look for the top of a step (> radius × 0.24)
@export var stand_height := 1.8
@export var crouch_height := 1.1
@export var crouch_rate := 8.0  ## metres per second the capsule shrinks/grows
@export var eye_below_top := 0.15

var crouching := false
var sprinting := false
var height := 1.8
var controls_enabled := true  ## false: ignores move/jump input (dead)

@onready var _shape: CollisionShape3D = $Collision
@onready var _head: Node3D = $Head


func _ready() -> void:
	_shape.shape = _shape.shape.duplicate()  # each player resizes its own capsule
	_set_height(stand_height)


func _physics_process(delta: float) -> void:
	var input := Input.get_vector("move_left", "move_right", "move_forward", "move_back") if controls_enabled else Vector2.ZERO
	_update_crouch(delta)
	sprinting = Input.is_action_pressed("sprint") and input.y < -0.5 and not crouching and is_on_floor()
	var speed := crouch_speed if crouching else (sprint_speed if sprinting else walk_speed)
	var wish := global_basis * Vector3(input.x, 0.0, input.y)
	wish.y = 0.0
	wish = wish.normalized() * input.length() * speed
	var blend := 1.0 - exp(-(ground_accel if is_on_floor() else air_accel) * delta)
	velocity.x = lerpf(velocity.x, wish.x, blend)
	velocity.z = lerpf(velocity.z, wish.z, blend)
	if is_on_floor():
		if controls_enabled and Input.is_action_just_pressed("jump") and not crouching:
			velocity.y = jump_velocity
	else:
		velocity.y -= gravity * delta
	var was_falling := not is_on_floor()
	var fall_speed := -velocity.y
	if not _step_up(delta):
		move_and_slide()
	if was_falling and is_on_floor():
		landed.emit(fall_speed)


## Something hit the player (an enemy's lunge). player_health.gd does the rest.
func take_hit(point: Vector3, force: Vector3, attacker: Node3D) -> void:
	hit_taken.emit(point, force, attacker)


## Hold crouch to crouch; let go and stand up again once there's room above.
func _update_crouch(delta: float) -> void:
	var want := Input.is_action_pressed("crouch")
	if not want and _blocked_above(stand_height - height):
		want = true
	crouching = want
	var target := crouch_height if crouching else stand_height
	if not is_equal_approx(height, target):
		_set_height(move_toward(height, target, crouch_rate * delta))


func _blocked_above(rise: float) -> bool:
	return rise > 0.01 and test_move(global_transform, Vector3.UP * rise)


## Resize the capsule keeping the feet where they are, and move the eyes with the top of the head.
func _set_height(h: float) -> void:
	height = h
	var capsule: CapsuleShape3D = _shape.shape
	capsule.height = h
	_shape.position.y = h * 0.5
	_head.position.y = h - eye_below_top


## Walking into something low (a stair, a rock): lift up, move across, settle down on top.
## Returns true when it stepped (then this tick doesn't move_and_slide).
func _step_up(delta: float) -> bool:
	if not is_on_floor() or velocity.y > 0.0:
		return false
	var motion := Vector3(velocity.x, 0.0, velocity.z) * delta
	if motion.length() < 0.0005 or not test_move(global_transform, motion):
		return false  # standing still, or nothing in the way
	var up := Vector3.UP * step_height
	if test_move(global_transform, up):
		return false  # no headroom
	# Probe a little further than this tick's motion: a slow tick only reaches the step's corner,
	# and touching a corner reads as a slope too steep to stand on. 12 cm past the face the capsule
	# meets the corner at ~41°, under floor_max_angle (50°).
	var ahead := motion.normalized() * maxf(motion.length(), step_probe)
	var raised := global_transform.translated(up)
	if test_move(raised, ahead):
		return false  # still blocked higher up: a wall, not a step
	var across := raised.translated(ahead)
	var hit := KinematicCollision3D.new()
	if not test_move(across, -up, hit):
		return false  # nothing to land on
	if hit.get_normal().angle_to(Vector3.UP) > floor_max_angle:
		return false  # too steep to stand on
	var before := global_position.y
	global_position = across.origin + hit.get_travel()
	stepped.emit(global_position.y - before)
	return true
