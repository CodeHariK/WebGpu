extends RigidBody3D

@export_group("Vehicle Properties")

@export var carmass: float = 100.0

@export var engine_force: float = 80.0
@export var brake_force: float = 20.0
@export var steering_angle_max_deg: float = 25.0

@export_group("Suspension")
@export var suspension_rest_length: float = 0.5
@export var spring_stiffness: float = 100.0
@export var damping_ratio: float = 0.5 # Between 0 (no damping) and 1 (critical damping)
var damper_coefficient

@export_group("Dimensions")
@export var wheelbase: float = 2.5 # Distance from front to rear axle
@export var track_width: float = 1.6 # Distance between left and right wheels
@export var wheel_radius: float = 0.4
@export var wheel_width: float = 0.25

var wheels: Array[Node3D] = []

# Inputs
var throttle_input: float = 0.0
var steering_input: float = 0.0
var brake_input: float = 0.0


# Called when the node enters the scene tree for the first time.
func _ready() -> void:
    self.mass = carmass

    var wheel_mass = carmass / 4.0
    damper_coefficient = 2.0 * damping_ratio * sqrt(spring_stiffness * wheel_mass)

    var div = 1.2
    var wheel_positions = [
        Vector3(track_width / div, 0, wheelbase / div),
        Vector3(-track_width / div, 0, wheelbase / div),
        Vector3(track_width / div, 0, -wheelbase / div),
        Vector3(-track_width / div, 0, -wheelbase / div)
    ]

    for pos in wheel_positions:
        var wheel_node = MeshInstance3D.new()
        
        # var cylinder_mesh = CylinderMesh.new()
        # cylinder_mesh.top_radius = wheel_radius
        # cylinder_mesh.bottom_radius = wheel_radius
        # cylinder_mesh.height = wheel_width
        
        # wheel_node.mesh = cylinder_mesh
        # # Rotate the cylinder to lie flat like a wheel
        # wheel_node.rotate_x(deg_to_rad(90))
        # wheel_node.rotate_y(deg_to_rad(90))

        wheel_node.position = pos
        
        add_child(wheel_node)
        wheels.append(wheel_node)


# Called every physics frame. 'delta' is the elapsed time since the previous frame.
func _physics_process(delta: float) -> void:
    # 1. Get player input (you'll need to set up these actions in Project Settings -> Input Map)
    # throttle_input = Input.get_axis("throttle_down", "throttle_up")
    # steering_input = Input.get_axis("steer_right", "steer_left")
    # brake_input = Input.get_action_strength("brake")
    var space_state = get_world_3d().direct_space_state

    # 2. Iterate through each wheel and apply forces
    for i in range(wheels.size()):
        var wheel: Node3D = wheels[i]

        var wheel_transform = wheel.global_transform
        var ray_origin = wheel_transform.origin
        var ray_direction = - global_transform.basis.y # Cast down from the vehicle's perspective
        var ray_end = ray_origin + ray_direction * suspension_rest_length

        if Engine.is_editor_hint() == false:
            DebugDraw3D.draw_line(ray_origin, ray_end, Color.RED)

        var query = PhysicsRayQueryParameters3D.create(ray_origin, ray_end)
        query.exclude = [get_rid()] # Don't collide with ourself

        var result = space_state.intersect_ray(query)

        if result:
            # --- Wheel is on the ground ---
            var hit_pos = result.position
            var distance = ray_origin.distance_to(hit_pos)

            # --- 1. Calculate Suspension Force ---
            var suspension_compression = suspension_rest_length - distance
            if suspension_compression > 0.0:
                var spring_force = spring_stiffness * suspension_compression

                # --- 2. Calculate Damper Force ---
                # Get the velocity at the wheel's contact point
                var wheel_velocity = linear_velocity + angular_velocity.cross(ray_origin - global_transform.origin)
                var damper_velocity = ray_direction.dot(wheel_velocity)
                var damper_force = damper_coefficient * damper_velocity
    

                # --- 3. Apply Total Suspension Force ---
                # The force is a combination of the spring and the damper
                var suspension_force_total = - ray_direction * (spring_force - damper_force)
                apply_force(suspension_force_total, wheel_transform.origin - global_transform.origin)

                # TODO: Add steering and friction forces here
