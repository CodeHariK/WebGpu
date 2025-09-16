extends Node3D

@onready var ball: RigidBody3D = $"."
@onready var car_mesh: MeshInstance3D = $MeshInstance3D
@onready var collision_shape_3d: CollisionShape3D = $CollisionShape3D
@onready var ground_ray: RayCast3D = $RayCast3D

var engine_force = 40.0
var turn_speed = 3.0
var drift_factor = 0.85
var jump_force = 20.0
var air_control = 0.3

var velocity = Vector3.ZERO
var steering = 0.0
var is_drifting = false

func _physics_process(delta):
    var is_grounded = ground_ray.is_colliding()
    
    # Basic movement
    var input = Vector3.ZERO
    input.x = Input.get_axis("steer_left", "steer_right")
    input.z = Input.get_axis("accelerate", "brake")
    
    # Apply engine force
    if is_grounded:
        velocity += -car_mesh.global_transform.basis.z * input.z * engine_force * delta
        
        # Steering
        if input.x != 0:
            var turn_amount = input.x * turn_speed * delta
            if is_drifting:
                turn_amount *= 1.5
            car_mesh.rotate_y(turn_amount)
    else:
        # Air control
        velocity += -car_mesh.global_transform.basis.z * input.z * engine_force * air_control * delta
    
    # Drifting
    if Input.is_action_pressed("drift") and is_grounded:
        is_drifting = true
        velocity = velocity.lerp(velocity.slide(car_mesh.transform.basis.x), drift_factor * delta)
    else:
        is_drifting = false
    
    # Jumping
    if Input.is_action_just_pressed("jump") and is_grounded:
        velocity.y = jump_force
    
    # Apply velocity to ball
    ball.linear_velocity = velocity
    
    # Update car mesh position to follow ball
# ...existing code...

    # Update car mesh position to follow ball
    car_mesh.global_position = ball.global_position
    car_mesh.position.y -= (collision_shape_3d.shape as SphereShape3D).radius # Adjust for ball radius
