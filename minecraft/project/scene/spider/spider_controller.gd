extends CharacterBody3D

const SPEED = 5
const JUMP_VELOCITY = 20
const STEP_DISTANCE = 1.2
const STEP_SPEED = 10.0
@export var GRAVITY = 30

@export_group("Head Look")
@export var ROTATE_HEAD = false
@export var MOUSE_SENSITIVITY = 0.005
@export var MAX_HEAD_X_ROT = 0.3
@export var HEAD_UP_DIS = 0.3

var head_rotation = Vector2()

@onready var body: CharacterBody3D = $"."
@onready var head_bone: BoneAttachment3D = $SpiderSkeleton/HeadBone

@onready var ray_cast_3d_ground: RayCast3D = $SpiderSkeleton/RayCast3D_GROUND
@onready var ray_cast_3d_fr: RayCast3D = $SpiderSkeleton/RayCast3D_FR
@onready var ray_cast_3d_fl: RayCast3D = $SpiderSkeleton/RayCast3D_FL
@onready var ray_cast_3d_br: RayCast3D = $SpiderSkeleton/RayCast3D_BR
@onready var ray_cast_3d_bl: RayCast3D = $SpiderSkeleton/RayCast3D_BL

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

var leg_collision_fr # Can be Vector3 or null
var leg_collision_fl # Can be Vector3 or null
var leg_collision_br # Can be Vector3 or null
var leg_collision_bl # Can be Vector3 or null

var _forward_dir: Vector3
var _right_dir: Vector3

func _ready() -> void:
    Input.mouse_mode = Input.MOUSE_MODE_CAPTURED

    ray_cast_3d_fr.force_raycast_update()
    ray_cast_3d_fl.force_raycast_update()
    ray_cast_3d_br.force_raycast_update()
    ray_cast_3d_bl.force_raycast_update()

    ik_target_fr = ray_cast_3d_fr.get_collision_point()
    leg_collision_fr = ik_target_fr
    ik_target_fl = ray_cast_3d_fl.get_collision_point()
    leg_collision_fl = ik_target_fl
    ik_target_br = ray_cast_3d_br.get_collision_point()
    leg_collision_br = ik_target_br
    ik_target_bl = ray_cast_3d_bl.get_collision_point()
    leg_collision_bl = ik_target_bl
    
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
    velocity += global_transform.basis.y.normalized() * -GRAVITY * delta

    # Handle jump.
    if Input.is_action_just_pressed("ui_accept"):
        velocity = global_transform.basis.y.normalized() * JUMP_VELOCITY

    # Get the input direction and handle the movement/deceleration.
    # As good practice, you should replace UI actions with custom gameplay actions.
    var input_dir := Input.get_vector("a", "d", "s", "w")
    
    # Calculate forward direction based on leg positions
    var front_midpoint = (leg_collision_fr + leg_collision_fl) / 2.0
    var back_midpoint = (leg_collision_br + leg_collision_bl) / 2.0
    # var front_midpoint = (ik_target_fr + ik_target_fl) / 2.0
    # var back_midpoint = (ik_target_br + ik_target_bl) / 2.0

    _forward_dir = (front_midpoint - back_midpoint).normalized()
    
    # Calculate the object's right direction, perpendicular to its forward direction
    _right_dir = _forward_dir.cross(Vector3.UP).normalized()
    
    var direction = (_forward_dir * input_dir.y + _right_dir * input_dir.x).normalized()
    if direction:
        velocity.x = direction.x * SPEED
        velocity.z = direction.z * SPEED
    else:
        velocity.x = move_toward(velocity.x, 0, SPEED)
        velocity.z = move_toward(velocity.z, 0, SPEED)

    move_and_slide()

    _debug_rays(front_midpoint, back_midpoint)

    _update_legs(delta)

    _update_head()

func _debug_rays(front_midpoint, back_midpoint) -> void:
    var debugsize = Vector3(.1, .1, .1)
    DebugDraw3D.draw_box(front_midpoint - debugsize / 2, Quaternion.IDENTITY, Vector3(.1, .1, .1))
    DebugDraw3D.draw_box(back_midpoint - debugsize / 2, Quaternion.IDENTITY, Vector3(.1, .1, .1))

    var body_center = head_bone.global_position

    DebugDraw3D.draw_arrow(body_center, body_center + _forward_dir, Color.CYAN, .1)
    DebugDraw3D.draw_arrow(body_center, body_center + _right_dir, Color.MAGENTA, .1)

    var up_dir = _forward_dir.cross(_right_dir).normalized()
    DebugDraw3D.draw_arrow(body_center, body_center + up_dir, Color.YELLOW, 0.1)

    var leg_base_center = (front_midpoint + back_midpoint) / 2.0
    DebugDraw3D.draw_line(leg_base_center, body_center, Color.ORANGE, 0.1)
    
    # Raycast from body center down to leg base center to find ground
    var space_state = get_world_3d().direct_space_state
    var down_direction = (leg_base_center - body_center)
    var down_query = PhysicsRayQueryParameters3D.create(body_center, body_center + down_direction.normalized() * 5)
    var down_result = space_state.intersect_ray(down_query)
    if down_result:
        DebugDraw3D.draw_line(body_center, down_result.position, Color.LIGHT_BLUE, 0.1)

    # Raycast from body_center forward to get wall normal
    var forward_query = PhysicsRayQueryParameters3D.create(body_center, body_center + _forward_dir * 4)
    # Exclude the spider itself from the raycast
    forward_query.exclude = [get_rid()]
    var forward_result = space_state.intersect_ray(forward_query)
    if forward_result:
        var collision_point = forward_result.position
        var collision_normal = forward_result.normal
        DebugDraw3D.draw_arrow(collision_point, collision_point + collision_normal, Color.WHITE, 0.1)
        DebugDraw3D.draw_arrow(collision_point, collision_point + collision_normal, Color.WHITE, 0.1, 0.1)

        # Project forward vector onto the collision plane to get the perpendicular direction
        var perpendicular_dir = _forward_dir - collision_normal * _forward_dir.dot(collision_normal)
        DebugDraw3D.draw_arrow(collision_point, collision_point + perpendicular_dir.normalized(), Color.RED, 0.1, 0.1)

func _update_legs(delta: float) -> void:
    # Determine if the planted group needs to step
    var wants_to_step = false
    if current_stepping_group == LegGroup.GROUP_A: # Group B is planted
        if (ray_cast_3d_fl.is_colliding() and ray_cast_3d_fl.get_collision_point().distance_to(ik_target_fl) > STEP_DISTANCE) or \
           (ray_cast_3d_br.is_colliding() and ray_cast_3d_br.get_collision_point().distance_to(ik_target_br) > STEP_DISTANCE):
            wants_to_step = true
    else: # Group A is planted
        if (ray_cast_3d_fr.is_colliding() and ray_cast_3d_fr.get_collision_point().distance_to(ik_target_fr) > STEP_DISTANCE) or \
           (ray_cast_3d_bl.is_colliding() and ray_cast_3d_bl.get_collision_point().distance_to(ik_target_bl) > STEP_DISTANCE):
            wants_to_step = true

    if wants_to_step:
        current_stepping_group = LegGroup.GROUP_A if current_stepping_group == LegGroup.GROUP_B else LegGroup.GROUP_B

    if ray_cast_3d_fr.is_colliding():
        leg_collision_fr = ray_cast_3d_fr.get_collision_point()
    else:
        leg_collision_fr = ik_target_fr
        # leg_collision_fr = null

    if ray_cast_3d_fl.is_colliding():
        leg_collision_fl = ray_cast_3d_fl.get_collision_point()
    else:
        leg_collision_fl = ik_target_fl
        # leg_collision_fl = null

    if ray_cast_3d_br.is_colliding():
        leg_collision_br = ray_cast_3d_br.get_collision_point()
    else:
        leg_collision_br = ik_target_br
        # leg_collision_br = null

    if ray_cast_3d_bl.is_colliding():
        leg_collision_bl = ray_cast_3d_bl.get_collision_point()
    else:
        leg_collision_bl = ik_target_bl
        # leg_collision_bl = null

    ik_target_fr = _update_leg_position(godot_ik_effector_fr, ray_cast_3d_fr, ik_target_fr, current_stepping_group == LegGroup.GROUP_A, delta)
    ik_target_bl = _update_leg_position(godot_ik_effector_bl, ray_cast_3d_bl, ik_target_bl, current_stepping_group == LegGroup.GROUP_A, delta)
    ik_target_fl = _update_leg_position(godot_ik_effector_fl, ray_cast_3d_fl, ik_target_fl, current_stepping_group == LegGroup.GROUP_B, delta)
    ik_target_br = _update_leg_position(godot_ik_effector_br, ray_cast_3d_br, ik_target_br, current_stepping_group == LegGroup.GROUP_B, delta)

func _update_leg_position(effector: GodotIKEffector, raycast: RayCast3D, current_target: Vector3, can_step: bool, delta: float) -> Vector3:
    var new_target = current_target
    if can_step and raycast.is_colliding():
        var ground_pos = raycast.get_collision_point()
        new_target = ground_pos

    effector.global_position = effector.global_position.lerp(new_target, STEP_SPEED * delta)

    DebugDraw3D.draw_line(raycast.global_position, new_target, Color.YELLOW, .05)

    return new_target

func _update_head() -> void:
    # Calculate the average position of the raycast origins
    var avg_ray_origin = (ray_cast_3d_fr.global_position + ray_cast_3d_fl.global_position + ray_cast_3d_bl.global_position + ray_cast_3d_br.global_position) / 4.0

    # Set the head bone's global position to the average raycast origin position, plus an offset to keep it above the ground
    head_bone.global_position = avg_ray_origin + Vector3.UP * HEAD_UP_DIS

    if ROTATE_HEAD:
        body.rotation = Vector3(head_rotation.x, head_rotation.y, 0)
