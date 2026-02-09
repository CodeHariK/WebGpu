extends CharacterBody3D

@export_group("Camera")
@export_range(0.0, 1.0) var mouse_sensitivity = 0.25

@export_group("Movement")
@export var move_speed := 8.0
@export var acceleration := 20.0
@export var rotation_speed := 12.0
@export var jump_impulse := 12.0

@onready var camera_pivot: Node3D = %CameraPivot
@onready var camera_3d: Camera3D = %Camera3D
@onready var sophia: Node3D = %sophia

var _camera_input_direction := Vector2.ZERO
var last_movement_direction := Vector3.BACK
var gravity = -30.0

const SPEED = 5.0
const JUMP_VELOCITY = 4.5

func _input(event: InputEvent) -> void:
    if event.is_action_pressed("left_click"):
        Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
    if event.is_action_pressed("ui_cancel"):
        Input.mouse_mode = Input.MOUSE_MODE_VISIBLE

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventMouseMotion and Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
        var motion_event := event as InputEventMouseMotion
        _camera_input_direction = motion_event.screen_relative * mouse_sensitivity

func _physics_process(delta: float) -> void:
    
    camera_pivot.rotation.x += _camera_input_direction.y * delta
    camera_pivot.rotation.x = clamp(camera_pivot.rotation.x, -PI/6, PI/3)
    
    camera_pivot.rotation.y -= _camera_input_direction.x * delta
    
    _camera_input_direction = Vector2.ZERO
    
    var raw_input := Input.get_vector("ui_left", "ui_right", "ui_up", "ui_down")
    var forward := camera_3d.global_basis.z
    var right := camera_3d.global_basis.x
    
    var move_direction := forward * raw_input.y + right * raw_input.x
    move_direction.y = 0.0
    move_direction = move_direction.normalized()
    
    var y_velocity := velocity.y
    velocity.y = 0.0
    velocity = velocity.move_toward(move_direction * move_speed, acceleration * delta)
    velocity.y = y_velocity + gravity * delta
    
    var is_starting_jump := Input.is_action_just_pressed("jump") and is_on_floor()
    if is_starting_jump:
        velocity.y += jump_impulse
    
    
    move_and_slide()
    
    if move_direction.length() > 0.2:
        last_movement_direction = move_direction
    var target_angle := Vector3.BACK.signed_angle_to(last_movement_direction, Vector3.UP)
    sophia.global_rotation.y = lerp_angle(sophia.rotation.y, target_angle, rotation_speed * delta)
    
    return
    
    ## Add the gravity.
    #if not is_on_floor():
        #velocity += get_gravity() * delta
#
    ## Handle jump.
    #if Input.is_action_just_pressed("ui_accept") and is_on_floor():
        #velocity.y = JUMP_VELOCITY
#
    ## Get the input direction and handle the movement/deceleration.
    ## As good practice, you should replace UI actions with custom gameplay actions.
    #var input_dir := Input.get_vector("ui_left", "ui_right", "ui_up", "ui_down")
    #var direction := (transform.basis * Vector3(input_dir.x, 0, input_dir.y)).normalized()
    #if direction:
        #velocity.x = direction.x * SPEED
        #velocity.z = direction.z * SPEED
    #else:
        #velocity.x = move_toward(velocity.x, 0, SPEED)
        #velocity.z = move_toward(velocity.z, 0, SPEED)
#
    #move_and_slide()
