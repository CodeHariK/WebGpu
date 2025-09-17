extends RigidBody3D

@onready var rigid_body_3d: RigidBody3D = $"."
@onready var mesh_3d: MeshInstance3D = $MeshInstance3D
@onready var collision_shape_3d: CollisionShape3D = $CollisionShape3D

@export var car_mass: float = 1200.0
@export var car_width: float = 2
@export var car_length: float = 4
@export var car_height: float = 1

@export var chassis_base_height: float = 0.5
@export var suspension_max_compression: float = 0.2
@export var front_wheel_radius: float = 0.35
@export var back_wheel_radius: float = 0.35

@export var suspension_rest_length: float = 0.4
@export var suspension_stiffness: float = 20000.0
@export var suspension_damping_ratio: float = 0.4

@export var wheel_x_ratio: float = 1.0 # lateral placement multiplier (width)
@export var wheel_y_ratio: float = 1.0 # longitudinal placement multiplier (length)

@export var debug_draw: bool = true

# internal
var wheel_local_offsets: Array = []
var debug_wheels: Array = []

func _ready() -> void:
    ##
    var transparent_mat = StandardMaterial3D.new()
    transparent_mat.albedo_color = Color(0.1, 0.6, 1.0, 1.0)
    transparent_mat.metallic = 0.1
    transparent_mat.roughness = 0.6
    transparent_mat.transparency = BaseMaterial3D.TRANSPARENCY_ALPHA
    transparent_mat.albedo_color.a = 0.5


    # set the physics mass
    mass = car_mass

    rigid_body_3d.mass = car_mass

    # Update mesh_3d to a new BoxMesh
    var box_mesh = BoxMesh.new()
    box_mesh.size = Vector3(car_width, car_height, car_length)
    mesh_3d.mesh = box_mesh
    mesh_3d.material_override = transparent_mat

    # Update collision_shape_3d to a new BoxShape3D
    var box_shape = BoxShape3D.new()
    box_shape.size = Vector3(car_width, car_height, car_length)
    collision_shape_3d.shape = box_shape

    # compute wheel local offsets (X=forward, Z=right)
    var half_w = (car_width * 0.5) * wheel_x_ratio
    var half_l = (car_length * 0.5) * wheel_y_ratio
    var half_h = - car_height * .5 + (suspension_rest_length + back_wheel_radius - chassis_base_height)

    wheel_local_offsets = [
        Vector3(+half_w, half_h, -half_l), # FR
        Vector3(-half_w, half_h, -half_l), # FL
        Vector3(+half_w, half_h, +half_l), # RR
        Vector3(-half_w, half_h, +half_l) # RL
    ]

    # create small debug meshes to represent tires (Sphere for simplicity)
    for i in wheel_local_offsets.size():
        var mi = MeshInstance3D.new()
        var sm = SphereMesh.new()
        # SphereMesh has radius, but we also scale to be safe
        if i < 2:
            sm.radius = front_wheel_radius
        else:
            sm.radius = back_wheel_radius
        sm.radial_segments = 12
        sm.rings = 8
        mi.mesh = sm
        mi.cast_shadow = GeometryInstance3D.SHADOW_CASTING_SETTING_OFF

        mi.material_override = transparent_mat

        mi.visible = !debug_draw
        add_child(mi)
        debug_wheels.append(mi)

func _physics_process(delta: float) -> void:
    var space = get_world_3d().direct_space_state

    # For each wheel: cast a ray downwards (world -Y) and debug draw the ray + tire
    for i in wheel_local_offsets.size():
        var local_off: Vector3 = wheel_local_offsets[i]

        # world position of the wheel hardpoint (no Y offset)
        var world_offset = global_transform.origin + (global_transform.basis * local_off)

        # start the ray at the wheel hardpoint position (no downward offset)
        var start = world_offset
        var radius = front_wheel_radius if i < 2 else back_wheel_radius
        var ray_length = suspension_rest_length + radius + 0.2
        var to = start + -global_transform.basis.y * ray_length

        # exclude the car itself
        var exclude = [self]
        var query = PhysicsRayQueryParameters3D.new()
        query.from = start
        query.to = to
        query.exclude = [self]
        var result = space.intersect_ray(query)

        if debug_draw:
            # draw full ray (green if hit, red if no hit)
            if result:
                var duration = .1
                DebugDraw3D.draw_line(start, result.position, Color(0.0, 1.0, 0.0), duration)
            #     DebugDraw3D.draw_line(result.position, result.position + result.normal * 0.25, Color(0.0, 0.6, 1.0))
            # else:
            #     DebugDraw3D.draw_line(start, to, Color(1.0, 0.2, 0.2))

        if result:
            var contact_pos: Vector3 = result.position
            var contact_normal: Vector3 = result.normal
            var distance = start.distance_to(contact_pos)

            # Compression is how much shorter the ray is than rest length
            var compression = suspension_rest_length + radius - distance
            compression = clamp(compression, 0.0, suspension_max_compression)

            var compression_ratio = compression / suspension_max_compression

            # Relative velocity of wheel along suspension direction
            var velocity_at_point = linear_velocity + angular_velocity.cross(contact_pos - global_transform.origin)
            var suspension_dir = contact_normal.normalized()
            var rel_vel = velocity_at_point.dot(suspension_dir)

            var wheel_mass = car_mass / wheel_local_offsets.size()
            var critical_damping = 2.0 * sqrt(suspension_stiffness * wheel_mass)
            var damping_coefficient = suspension_damping_ratio * critical_damping

            # Forces
            var spring = suspension_stiffness * compression
            var damping = damping_coefficient * rel_vel
            var total_force = (spring - damping) * suspension_dir

            apply_force(total_force, contact_pos - global_transform.origin)

            if debug_draw:
                DebugDraw3D.draw_text(contact_pos + contact_normal * 0.5 + Vector3.UP * 0.3 + Vector3.LEFT, String.num(compression_ratio, 3))

            # position the debug wheel at contact + normal*wheel_radius
            var tire_pos = contact_pos + contact_normal * radius
            debug_wheels[i].global_transform = Transform3D(debug_wheels[i].global_transform.basis, tire_pos)

        else:
            # place at the end of ray (no ground)
            debug_wheels[i].global_transform = Transform3D(debug_wheels[i].global_transform.basis, to)


    #####################################
    # Driving and steering input handling
    var throttle_input = Input.get_axis("brake", "accelerate")
    var steering_input = Input.get_axis("steer_left", "steer_right")
    var drive_force_magnitude = 5000.0

    # Apply drive force at rear wheels (indices 2 and 3)
    var forward_dir = - transform.basis.z.normalized() # local car forward
    for i in [2, 3]:
        var wheel_world_pos = global_transform.origin + (global_transform.basis * wheel_local_offsets[i])
        var drive_force = forward_dir * throttle_input * drive_force_magnitude * 0.5
        apply_force(drive_force, wheel_world_pos - global_transform.origin)

        if debug_draw and drive_force.length() > 0.01:
            DebugDraw3D.draw_arrow(wheel_world_pos, wheel_world_pos + drive_force * 0.001, Color(1, 0.5, 0))

    # Apply steering torque proportional to steering input and current speed
    var speed = linear_velocity.length()
    var steering_torque_magnitude = 1500.0
    var torque = - Vector3.UP * steering_input * steering_torque_magnitude * speed
    apply_torque(torque)
    if debug_draw and torque.length() > 0.01:
        DebugDraw3D.draw_arrow(global_transform.origin, global_transform.origin + torque.normalized(), Color(0.5, 1, 0))
