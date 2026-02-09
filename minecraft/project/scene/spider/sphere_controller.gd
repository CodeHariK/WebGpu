# /Users/Shared/Code/WebGpu/minecraft/project/scene/spider/surface_walker_controller.gd
extends CharacterBody3D

@export_group("Movement")
@export var SPEED = 8.0
@export var ACCELERATION = 8.0
@export var DECELERATION = 10.0
@export var GRAVITY = 40.0

@export_group("Surface Detection")
@export var SURFACE_STICK_FORCE = 20.0
@export var RAY_LENGTH = 1.5
@export var ROTATION_LERP_SPEED = 50

@export_group("Camera")
@export var MOUSE_SENSITIVITY = 0.004

# A set of directions to cast rays in to find the ground.
# More rays give smoother orientation but cost more performance.
var RAY_DIRECTIONS = [
    Vector3.UP, Vector3.DOWN, Vector3.LEFT, Vector3.RIGHT, Vector3.FORWARD, Vector3.BACK,
    Vector3(1, 1, 1).normalized(), Vector3(1, 1, -1).normalized(),
    Vector3(1, -1, 1).normalized(), Vector3(1, -1, -1).normalized(),
    Vector3(-1, 1, 1).normalized(), Vector3(-1, 1, -1).normalized(),
    Vector3(-1, -1, 1).normalized(), Vector3(-1, -1, -1).normalized(),

    # Edge directions (12)
    Vector3(1, 1, 0).normalized(), Vector3(1, -1, 0).normalized(),
    Vector3(-1, 1, 0).normalized(), Vector3(-1, -1, 0).normalized(),
    Vector3(1, 0, 1).normalized(), Vector3(1, 0, -1).normalized(),
    Vector3(-1, 0, 1).normalized(), Vector3(-1, 0, -1).normalized(),
    Vector3(0, 1, 1).normalized(), Vector3(0, 1, -1).normalized(),
    Vector3(0, -1, 1).normalized(), Vector3(0, -1, -1).normalized(),
]

var up_dir: Vector3 = Vector3.UP
var forward_dir: Vector3 = Vector3.FORWARD
var right_dir: Vector3 = Vector3.RIGHT
var y_rot: float = 0.0
var is_on_surface: bool = false

var all_cameras: Array[Camera3D] = []
var current_camera_index: int = 0


func _ready() -> void:
    Input.mouse_mode = Input.MOUSE_MODE_CAPTURED
    
    var root_node = get_tree().get_root()
    all_cameras = find_all_camera3d_nodes(root_node)

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventMouseMotion:
        # Accumulate horizontal mouse movement to control the forward direction.
        y_rot -= event.relative.x * MOUSE_SENSITIVITY
    
    if event.is_action_pressed("toggle_camera") and not all_cameras.is_empty():
        all_cameras[current_camera_index].current = false
        current_camera_index = (current_camera_index + 1) % all_cameras.size()
        all_cameras[current_camera_index].current = true
    
    if event.is_action_pressed("ui_cancel"):
        Input.mouse_mode = Input.MOUSE_MODE_CAPTURED if Input.mouse_mode == Input.MOUSE_MODE_VISIBLE else Input.MOUSE_MODE_VISIBLE


func _physics_process(delta: float) -> void:
    # --- 1. Surface Normal Calculation ---
    _update_surface_normal(delta)

    forward_dir = - transform.basis.z
    forward_dir = (forward_dir - up_dir * forward_dir.dot(up_dir)).normalized()
    forward_dir = forward_dir.rotated(up_dir, y_rot)
    y_rot = 0.0
    DebugDraw3D.draw_arrow(global_position, global_position + forward_dir, Color.BLUE, 0.1)

    right_dir = forward_dir.cross(up_dir)
    DebugDraw3D.draw_arrow(global_position, global_position + right_dir, Color.RED, 0.1)

    var input_dir := Input.get_vector("a", "d", "s", "w")
    
    # Calculate movement direction relative to the sphere's current orientation.
    var direction = (forward_dir * input_dir.y + right_dir * input_dir.x).normalized()

    # Apply acceleration/deceleration.
    if direction:
        velocity = velocity.lerp(direction * SPEED, ACCELERATION * delta)
    else:
        velocity = velocity.lerp(Vector3.ZERO, DECELERATION * delta)

    if is_on_surface:
        pass
        # # Apply a constant "gravity" force towards the surface to stick to it.
        # velocity += -up_dir * SURFACE_STICK_FORCE * delta
    else:
        # Apply standard downward gravity when in the air.
        velocity += Vector3.DOWN * GRAVITY * delta
    
    move_and_slide()

    var target_basis = Basis(right_dir, up_dir, -forward_dir)
    transform.basis = transform.basis.slerp(target_basis, ROTATION_LERP_SPEED * delta)


func _update_surface_normal(delta: float) -> void:
    var space_state = get_world_3d().direct_space_state
    var average_normal = Vector3.ZERO
    var total_weight = 0.0

    # Cast rays in all directions to find nearby surfaces.
    for dir in RAY_DIRECTIONS:
        var query = PhysicsRayQueryParameters3D.create(global_position, global_position + dir * RAY_LENGTH)
        query.exclude = [get_rid()]
        var result = space_state.intersect_ray(query)

        if !result.is_empty():
            var distance = global_position.distance_to(result.position)
            # Weight is inversely proportional to distance. Closer hits have more influence.
            # The `pow(..., 2)` makes the falloff more pronounced.
            var weight = pow(1.0 - (distance / RAY_LENGTH), 2)
            average_normal += result.normal * weight
            total_weight += weight
            DebugDraw3D.draw_line(global_position, result.position, Color.WHITE_SMOKE)

    # If we hit any surfaces, calculate the average normal.
    if total_weight > 0:
        is_on_surface = true
        var target_up_dir = (average_normal / total_weight).normalized()
        up_dir = up_dir.slerp(target_up_dir, ROTATION_LERP_SPEED * delta)
        DebugDraw3D.draw_arrow(global_position, global_position + up_dir * 1, Color.GREEN, 0.1)
    else:
        is_on_surface = false
        # When in the air, smoothly rotate back to world up.
        up_dir = up_dir.slerp(Vector3.UP, ROTATION_LERP_SPEED * delta)

    # var query = PhysicsRayQueryParameters3D.create(global_position, global_position - up_dir * RAY_LENGTH)
    # query.exclude = [get_rid()]
    # var result = space_state.intersect_ray(query)
    # if !result.is_empty(): # Check if the ray hit something
    #     var collision_point = result.position
        
    #     DebugDraw3D.draw_sphere(collision_point, 0.1, Color.ORANGE)


func find_all_camera3d_nodes(node: Node) -> Array[Camera3D]:
    var found_cameras: Array[Camera3D] = []
    for child in node.get_children():
        if child is Camera3D:
            found_cameras.append(child)
        # Recursively search in child nodes
        found_cameras.append_array(find_all_camera3d_nodes(child))
    return found_cameras
