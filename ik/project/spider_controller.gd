extends CharacterBody3D

const SPEED = 5.0
const JUMP_VELOCITY = 2.5
const STEP_DISTANCE = 1.2
const STEP_SPEED = 10.0

@export_group("Head Look")
@export var MOUSE_SENSITIVITY = 0.005
@export var MAX_HEAD_X_ROT = 0.3

var head_rotation = Vector2()

@onready var head_bone: BoneAttachment3D = $SpiderSkeleton/HeadBone

@onready var ray_cast_3d_fr: RayCast3D = $SpiderSkeleton/HeadBone/RayCast3D_FR
@onready var ray_cast_3d_fl: RayCast3D = $SpiderSkeleton/HeadBone/RayCast3D_FL
@onready var ray_cast_3d_br: RayCast3D = $SpiderSkeleton/HeadBone/RayCast3D_BR
@onready var ray_cast_3d_bl: RayCast3D = $SpiderSkeleton/HeadBone/RayCast3D_BL

@onready var head: MeshInstance3D = $SpiderSkeleton/HeadBone/Head

@onready var godot_ik_effector_fr: GodotIKEffector = $SpiderSkeleton/GodotIK/GodotIKEffector_FR
@onready var godot_ik_effector_fl: GodotIKEffector = $SpiderSkeleton/GodotIK/GodotIKEffector_FL
@onready var godot_ik_effector_bl: GodotIKEffector = $SpiderSkeleton/GodotIK/GodotIKEffector_BL
@onready var godot_ik_effector_br: GodotIKEffector = $SpiderSkeleton/GodotIK/GodotIKEffector_BR

enum LegGroup {GROUP_A, GROUP_B} # GROUP_A = FR/BL, GROUP_B = FL/BR
var current_stepping_group = LegGroup.GROUP_A

var ik_target_fr: Vector3
var ik_target_fl: Vector3
var ik_target_br: Vector3
var ik_target_bl: Vector3

func _ready() -> void:
    Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

    ik_target_fr = ray_cast_3d_fr.get_collision_point()
    ik_target_fl = ray_cast_3d_fl.get_collision_point()
    ik_target_br = ray_cast_3d_br.get_collision_point()
    ik_target_bl = ray_cast_3d_bl.get_collision_point()

func _unhandled_input(event: InputEvent) -> void:
    if event is InputEventMouseMotion:
        if Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
            head_rotation.y -= event.relative.x * MOUSE_SENSITIVITY
            head_rotation.x -= event.relative.y * MOUSE_SENSITIVITY
            head_rotation.x = clamp(head_rotation.x, -MAX_HEAD_X_ROT, MAX_HEAD_X_ROT)
    
    if event.is_action_pressed("ui_cancel"):
        if Input.mouse_mode == Input.MOUSE_MODE_CAPTURED:
            Input.mouse_mode = Input.MOUSE_MODE_VISIBLE
        else:
            Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

func _physics_process(delta: float) -> void:
    # Add the gravity.
    if not is_on_floor():
        velocity += get_gravity() * delta

    # Handle jump.
    if Input.is_action_just_pressed("ui_accept") and is_on_floor():
        velocity.y = JUMP_VELOCITY

    # Get the input direction and handle the movement/deceleration.
    # As good practice, you should replace UI actions with custom gameplay actions.
    var camera_basis = get_viewport().get_camera_3d().global_transform.basis
    var input_dir := Input.get_vector("Left", "Right", "Down", "Up")
    
    var forward = - camera_basis.z
    forward.y = 0
    var right = camera_basis.x
    
    var direction = (forward * input_dir.y + right * input_dir.x).normalized()
    if direction:
        velocity.x = direction.x * SPEED
        velocity.z = direction.z * SPEED
    else:
        velocity.x = move_toward(velocity.x, 0, SPEED)
        velocity.z = move_toward(velocity.z, 0, SPEED)

    move_and_slide()

    # Determine if the planted group needs to step
    var wants_to_step = false
    if current_stepping_group == LegGroup.GROUP_A: # Group B is planted
        if ray_cast_3d_fl.get_collision_point().distance_to(ik_target_fl) > STEP_DISTANCE or \
           ray_cast_3d_br.get_collision_point().distance_to(ik_target_br) > STEP_DISTANCE:
            wants_to_step = true
    else: # Group A is planted
        if ray_cast_3d_fr.get_collision_point().distance_to(ik_target_fr) > STEP_DISTANCE or \
           ray_cast_3d_bl.get_collision_point().distance_to(ik_target_bl) > STEP_DISTANCE:
            wants_to_step = true

    if wants_to_step:
        current_stepping_group = LegGroup.GROUP_A if current_stepping_group == LegGroup.GROUP_B else LegGroup.GROUP_B

    ik_target_fr = _update_leg_position(godot_ik_effector_fr, ray_cast_3d_fr, ik_target_fr, current_stepping_group == LegGroup.GROUP_A, delta)
    ik_target_bl = _update_leg_position(godot_ik_effector_bl, ray_cast_3d_bl, ik_target_bl, current_stepping_group == LegGroup.GROUP_A, delta)
    ik_target_fl = _update_leg_position(godot_ik_effector_fl, ray_cast_3d_fl, ik_target_fl, current_stepping_group == LegGroup.GROUP_B, delta)
    ik_target_br = _update_leg_position(godot_ik_effector_br, ray_cast_3d_br, ik_target_br, current_stepping_group == LegGroup.GROUP_B, delta)

    head_bone.rotation = Vector3(head_rotation.x, head_rotation.y, 0)

    _draw_debug_rays()

func _update_leg_position(effector: GodotIKEffector, raycast: RayCast3D, current_target: Vector3, can_step: bool, delta: float) -> Vector3:
    var new_target = current_target
    if can_step and raycast.is_colliding():
        var ground_pos = raycast.get_collision_point()
        new_target = ground_pos

    effector.global_position = effector.global_position.lerp(new_target, STEP_SPEED * delta)
    return new_target

func _draw_debug_rays() -> void:
    # Assumes an autoloaded singleton named DebugDraw exists.
    # Replace with your actual debug drawing implementation if it's different.
    if not Engine.has_singleton("DebugDraw3D"):
        return

    _draw_ray(ray_cast_3d_fr, Color.RED)
    _draw_ray(ray_cast_3d_fl, Color.GREEN)
    _draw_ray(ray_cast_3d_br, Color.BLUE)
    _draw_ray(ray_cast_3d_bl, Color.YELLOW)

func _draw_ray(raycast: RayCast3D, color: Color) -> void:
    var start_pos = raycast.global_position
    var end_pos = raycast.to_global(raycast.target_position)

    DebugDraw3D.draw_line(start_pos, end_pos, color)

    # Draw a small cross at the end point
    var cross_size = 0.05
    DebugDraw3D.draw_line(end_pos - Vector3.RIGHT * cross_size, end_pos + Vector3.RIGHT * cross_size, color)
    DebugDraw3D.draw_line(end_pos - Vector3.UP * cross_size, end_pos + Vector3.UP * cross_size, color)
    DebugDraw3D.draw_line(end_pos - Vector3.FORWARD * cross_size, end_pos + Vector3.FORWARD * cross_size, color)
