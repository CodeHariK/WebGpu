extends CharacterBody3D

@export var speed = 5.0
@export var jump_velocity = 4.5
@export var acceleration = 8.0
@export var air_acceleration = 4.0
@export var friction = 10.0

# Get the gravity from the project settings to be synced with RigidBody nodes.
@export var gravity: float = ProjectSettings.get_setting("physics/3d/default_gravity")

func _physics_process(delta: float) -> void:
    var target_velocity := velocity

    # Add the gravity.
    if not is_on_floor():
        target_velocity.y -= gravity * delta

    # Handle Jump.
    if Input.is_action_just_pressed("ui_accept") and is_on_floor():
        target_velocity.y = jump_velocity

    # Get the input direction and handle the movement/deceleration.
    # As good practice, you should replace UI actions with custom gameplay actions.
    var input_dir := Input.get_vector("ui_left", "ui_right", "ui_up", "ui_down")
    var direction := (transform.basis * Vector3(input_dir.x, 0, input_dir.y)).normalized()
    
    var current_acceleration = air_acceleration if not is_on_floor() else acceleration
    
    if direction:
        target_velocity.x = direction.x * speed
        target_velocity.z = direction.z * speed
    else:
        target_velocity.x = move_toward(target_velocity.x, 0, speed)
        target_velocity.z = move_toward(target_velocity.z, 0, speed)

    # Interpolate for smoother movement
    velocity.x = lerp(velocity.x, target_velocity.x, current_acceleration * delta)
    velocity.z = lerp(velocity.z, target_velocity.z, current_acceleration * delta)
    velocity.y = target_velocity.y # Gravity and jump are not interpolated

    move_and_slide()

    # --- IDEAS FOR YOUR NEXT STEPS ---

    # 1. Airdash:
    # if Input.is_action_just_pressed("air_dash") and not is_on_floor():
    #     var dash_direction = direction if direction != Vector3.ZERO else transform.basis.z
    #     velocity = dash_direction * 20.0 # Set a high, non-persistent velocity

    # 2. Aim-Jump (assuming you have a camera):
    # if Input.is_action_just_pressed("ui_accept"):
    #     var aim_direction = $Camera3D.global_transform.basis.z
    #     velocity = aim_direction * jump_velocity
