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

enum Drivetrain {FRONT_WHEEL, REAR_WHEEL}

@export var drivetrain: Drivetrain = Drivetrain.REAR_WHEEL

@export var front_grip_curve: Curve
@export var rear_grip_curve: Curve
@export var max_slip_speed: float = 20.0

@export var wheel_x_ratio: float = 1.0 # lateral placement multiplier (width)
@export var wheel_y_ratio: float = 1.0 # longitudinal placement multiplier (length)

@export var debug_draw: bool = true

@export var steering_torque_magnitude = 1500.0
@export var max_engine_torque: float = 400.0 # Nm
@export var engine_rpm_idle: float = 1000.0
@export var engine_rpm_max: float = 6000.0
@export var engine_inertia: float = 0.1
@export var acceleration_rate: float = 1.0 # how fast throttle builds

var current_throttle: float = 0.0
var engine_rpm: float = engine_rpm_idle

@onready var vbox: VBoxContainer = $"../CanvasLayer/Control/VBoxContainer"

var wheel_local_offsets: Array = []
var debug_wheels: Array = []

var ui_label_rpm
var ui_label_torque_factor
var ui_label_force
var ui_label_slip

func ui() -> void:
    ui_label_rpm = Label.new()
    vbox.add_child(ui_label_rpm)

    ui_label_torque_factor = Label.new()
    vbox.add_child(ui_label_torque_factor)

    ui_label_force = Label.new()
    vbox.add_child(ui_label_force)

    ui_label_slip = Label.new()
    vbox.add_child(ui_label_slip)

func _ready() -> void:
    ui()

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
    # Set linear and angular damping for stability
    rigid_body_3d.linear_damp = .4
    rigid_body_3d.angular_damp = .8

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
    var grounded_wheels = 0

    # Input handling with throttle ramp
    var raw_throttle = Input.get_axis("brake", "accelerate")
    var steering_input = Input.get_axis("steer_left", "steer_right")

    var current_up = global_transform.basis.y.normalized()
    var target_up = Vector3.UP
    var y_alignment = current_up.dot(target_up)

    # Smooth throttle buildup
    if raw_throttle > current_throttle:
        current_throttle = min(raw_throttle, current_throttle + acceleration_rate * delta)
    elif raw_throttle < current_throttle:
        current_throttle = max(raw_throttle, current_throttle - acceleration_rate * delta)

    # Estimate wheel RPM from speed
    var wheel_radius_avg = (front_wheel_radius + back_wheel_radius) * 0.5
    var wheel_rpm = (linear_velocity.length() / (2.0 * PI * wheel_radius_avg)) * 60.0
    engine_rpm = clamp(wheel_rpm * 4.0, engine_rpm_idle, engine_rpm_max) # assume gear ratio of 4 for now

    ui_label_rpm.text = "rpm " + String.num(wheel_rpm, 3) + "  " + str(engine_rpm)

    # Torque curve: more torque at low RPM, taper at high RPM
    var torque_factor = 1.0 - (engine_rpm - engine_rpm_idle) / (engine_rpm_max - engine_rpm_idle)
    torque_factor = clamp(torque_factor, 0.2, 1.0)
    ui_label_torque_factor.text = str(torque_factor)
    var engine_torque = max_engine_torque * torque_factor * current_throttle

    # Convert torque to drive force at wheels
    var drive_force_value = engine_torque / wheel_radius_avg

    ui_label_force.text = "force " + str(drive_force_value)

    var forward_dir = - transform.basis.z.normalized() # local car forward
    var speed = linear_velocity.length()
    var torque = - Vector3.UP * steering_input * steering_torque_magnitude * speed

    var space = get_world_3d().direct_space_state
    var right_dir = transform.basis.x.normalized()

    # For each wheel: cast a ray downwards (world -Y) and debug draw the ray + tire
    for i in wheel_local_offsets.size():
        var local_off: Vector3 = wheel_local_offsets[i]

        # start the ray at the wheel hardpoint position (no downward offset)
        var start = global_transform.origin + (global_transform.basis * local_off)
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
            grounded_wheels += 1
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

            var force_aligned = (start - to).dot(Vector3.UP) > .9

            ##### Suspension
            if force_aligned:
                apply_force(total_force, contact_pos - global_transform.origin)

                if debug_draw:
                    DebugDraw3D.draw_text(contact_pos + contact_normal * 0.5 + Vector3.UP * 0.3 + Vector3.LEFT, String.num(compression_ratio, 3))

            # position the debug wheel at contact + normal*wheel_radius
            var tire_pos = contact_pos + contact_normal * radius
            debug_wheels[i].global_transform = Transform3D(debug_wheels[i].global_transform.basis, tire_pos)


            var is_drive_wheel = false
            if drivetrain == Drivetrain.REAR_WHEEL and (i == 2 or i == 3):
                is_drive_wheel = true
            elif drivetrain == Drivetrain.FRONT_WHEEL and (i == 0 or i == 1):
                is_drive_wheel = true

            ###### Acceleration
            if force_aligned and is_drive_wheel:
                var wheel_world_pos = global_transform.origin + (global_transform.basis * wheel_local_offsets[i])
                var drive_force = forward_dir * drive_force_value * 0.5
                apply_force(drive_force, wheel_world_pos - global_transform.origin)

                if debug_draw and drive_force.length() > 0.01:
                    DebugDraw3D.draw_line(wheel_world_pos, wheel_world_pos + drive_force * 0.01, Color(1, 0.5, 0))
            
            # Lateral/sliding force at this wheel if grounded
            var velocity_at_wheel = linear_velocity + angular_velocity.cross(contact_pos - global_transform.origin)
            var lat_speed = velocity_at_wheel.dot(right_dir)
            var slip = clamp(abs(lat_speed) / max_slip_speed, 0.0, 1.0)
            var grip_factor = 1.0
            if i < 2:
                grip_factor = front_grip_curve.sample(slip) if front_grip_curve else 1.0
            else:
                grip_factor = rear_grip_curve.sample(slip) if rear_grip_curve else 1.0

            var lat_force = - right_dir * lat_speed * (car_mass / wheel_local_offsets.size()) * grip_factor
            apply_force(lat_force, contact_pos - global_transform.origin)

            if debug_draw and lat_force.length() > 0.01:
                DebugDraw3D.draw_line(contact_pos, contact_pos + lat_force * 0.01, Color(0, 0, 1))
        else:
            # place at the end of ray (no ground)
            debug_wheels[i].global_transform = Transform3D(debug_wheels[i].global_transform.basis, to)


    var local_velocity: Vector3 = global_transform.basis.inverse() * linear_velocity
    if grounded_wheels > 0 and abs(local_velocity.z) > 0:
        apply_torque(torque)
        if debug_draw and torque.length() > 0.01:
            DebugDraw3D.draw_line(global_transform.origin, global_transform.origin + torque.normalized(), Color(0.5, 1, 0))

    # Auto-flip if car is upside down: rotate around local Z until up aligns with world up
    if Input.is_action_pressed("jump") and y_alignment < 0.5 and grounded_wheels < 3:
        # Give a small upward impulse to lift the car for clearance
        apply_central_impulse((Vector3(.0004, .5, .0004)) * car_mass)
        # Find rotation axis (local Z) and angle difference
        var axis = global_transform.basis.z.normalized()
        var angle = acos(clamp(y_alignment, -1.0, 1.0))
        # Slerp by rotating a bit each frame
        var rotation_step = angle * delta * 2.0
        global_transform.basis = Basis(axis, rotation_step) * global_transform.basis
