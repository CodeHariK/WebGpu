extends CharacterBody3D

@onready var animation_player: AnimationPlayer = $sophia/AnimationPlayer
@onready var animation_tree: AnimationTree = $AnimationTree
@onready var state_machine: AnimationNodeStateMachinePlayback = animation_tree.get("parameters/StateMachine/playback")
@onready var move_tilt_path: String = "parameters/StateMachine/Move/Add3/add_amount"

@onready var sophia: Node3D = $sophia

var run_tilt = 0.0: set = _set_run_tilt

@export_group("Movement")
@export var move_speed := 8.0
@export var acceleration := 20.0
@export var rotation_speed := 12.0
@export var jump_impulse := 12.0
@export var stopping_speed := 1.0
var _gravity := -30.0

var _camera_input_direction := Vector2.ZERO
var _last_movement_direction := Vector3.BACK
var _was_on_floor_last_frame := true

@onready var _last_input_direction := global_basis.z

@onready var _landing_sound: AudioStreamPlayer3D = %LandingSound
@onready var _jump_sound: AudioStreamPlayer3D = %JumpSound
@onready var _dust_particles: GPUParticles3D = %DustParticles

func _set_run_tilt(value: float):
	run_tilt = clamp(value, -1.0, 1.0)
	animation_tree.set(move_tilt_path, run_tilt)

@onready var camera_3d: Camera3D = $Camera3D

# Called when the node enters the scene tree for the first time.
func _ready() -> void:
	pass # Replace with function body.


# Called every frame. 'delta' is the elapsed time since the previous frame.
func _process(delta: float) -> void:
	pass


func _input(event: InputEvent) -> void:
	if event.is_action_pressed("ui_cancel"):
		Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
	elif event.is_action_pressed("left_click"):
		Input.mouse_mode = Input.MOUSE_MODE_CAPTURED


func _unhandled_input(event: InputEvent) -> void:
	var is_camera_motion := (
		event is InputEventMouseMotion and
		Input.get_mouse_mode() == Input.MOUSE_MODE_CAPTURED
	)

func _physics_process(delta: float) -> void:

	# Calculate movement input and align it to the camera's direction.
	var raw_input := Input.get_vector("move_left", "move_right", "move_up", "move_down", 0.4)
	# Should be projected onto the ground plane.
	var forward := camera_3d.global_basis.z
	var right := camera_3d.global_basis.x
	var move_direction := forward * raw_input.y + right * raw_input.x
	move_direction.y = 0.0
	move_direction = move_direction.normalized()

	# To not orient the character too abruptly, we filter movement inputs we
	# consider when turning the skin. This also ensures we have a normalized
	# direction for the rotation basis.
	if move_direction.length() > 0.2:
		_last_input_direction = move_direction.normalized()
	var target_angle := Vector3.BACK.signed_angle_to(_last_input_direction, Vector3.UP)
	sophia.global_rotation.y = lerp_angle(sophia.rotation.y, target_angle, rotation_speed * delta)

	# We separate out the y velocity to only interpolate the velocity in the
	# ground plane, and not affect the gravity.
	var y_velocity := velocity.y
	velocity.y = 0.0
	velocity = velocity.move_toward(move_direction * move_speed, acceleration * delta)
	if is_equal_approx(move_direction.length_squared(), 0.0) and velocity.length_squared() < stopping_speed:
		velocity = Vector3.ZERO
	velocity.y = y_velocity + _gravity * delta

	# Character animations and visual effects.
	var ground_speed := Vector2(velocity.x, velocity.z).length()
	var is_just_jumping := Input.is_action_just_pressed("jump") and is_on_floor()
	if is_just_jumping:
		velocity.y += jump_impulse
		jump()
		_jump_sound.play()
	elif not is_on_floor() and velocity.y < 0:
		fall()
	elif is_on_floor():
		if ground_speed > 0.0:
			move()
		else:
			idle()

	# Calculate and apply run tilt based on horizontal movement input
	if is_on_floor() and ground_speed > 0.1:
		# If moving right, raw_input.x is positive; if left, negative.
		# Adjust the lerp speed (10 * delta) to change how quickly she tilts.
		run_tilt = lerp(run_tilt, raw_input.x, 10.0 * delta)
	else:
		# Return to center when standing still or in mid-air
		run_tilt = lerp(run_tilt, 0.0, 10.0 * delta)

	_dust_particles.emitting = is_on_floor() && ground_speed > 0.0

	if is_on_floor() and not _was_on_floor_last_frame:
		_landing_sound.play()

	_was_on_floor_last_frame = is_on_floor()
	move_and_slide()


func idle():
	state_machine.travel("Idle")

func move():
	state_machine.travel("Move")

func fall():
	state_machine.travel("Fall")

func jump():
	state_machine.travel("Jump")

func edge_grab():
	state_machine.travel("EdgeGrab")

func wall_slide():
	state_machine.travel("WallSlide")
