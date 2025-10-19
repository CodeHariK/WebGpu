@tool
class_name TransformGizmo
extends Node3D

## A callback function to be executed when the gizmo's transform is updated by the user.
## The callback will receive the new Transform3D as its only argument.
@export var update_callback: Callable

enum ManipulationMode {
    NONE,
    MOVE,
    ROTATE,
    SCALE
}

var MOVE_GIZMO_LEN: float = .5
var SCALE_GIZMO_LEN: float = 1.25

@export_group("Manipulation")
@export var _manipulation_mode := ManipulationMode.NONE
@export_group("Snapping")
@export var move_snap: float = 0.0
@export var rotate_snap: float = 0.0 # In degrees
@export var scale_snap: float = 0.0

var _drag_axis: Vector3
var _drag_plane := Plane()
var _drag_start_position: Vector3
var _drag_start_vector: Vector3
var _last_transform: Transform3D
var _drag_start_transform: Transform3D

var circle_shader: Shader = preload("res://scene/road/circle_gizmo.gdshader")

func _init() -> void:
    # This ensures the gizmo runs its _process loop in the editor.
    set_process_mode(Node.PROCESS_MODE_PAUSABLE)

    # _create_box_gizmo("MoveX", Color(1, 0.2, 0.2, 1), Transform3D.IDENTITY.rotated(Vector3.FORWARD, PI / 2.0),
    #                     ManipulationMode.MOVE, Vector3(0, MOVE_GIZMO_LEN / 2 + SCALE_GIZMO_LEN / 2, 0), Vector2(.06, MOVE_GIZMO_LEN))
    # _create_box_gizmo("MoveY", Color(0.6, 1, 0.2, 1), Transform3D.IDENTITY,
    #                     ManipulationMode.MOVE, Vector3(0, MOVE_GIZMO_LEN / 2 + SCALE_GIZMO_LEN / 2, 0), Vector2(.06, MOVE_GIZMO_LEN))
    # _create_box_gizmo("MoveZ", Color(0.5, 0.5, 1, 1), Transform3D.IDENTITY.rotated(Vector3.RIGHT, PI / 2.0),
    #                     ManipulationMode.MOVE, Vector3(0, MOVE_GIZMO_LEN / 2 + SCALE_GIZMO_LEN / 2, 0), Vector2(.06, MOVE_GIZMO_LEN))

    # _create_box_gizmo("ScaleX", Color(1, 0.2, 0.2, 1), Transform3D.IDENTITY.rotated(Vector3.FORWARD, PI / 2.0),
    #                     ManipulationMode.SCALE, Vector3(0, MOVE_GIZMO_LEN + SCALE_GIZMO_LEN / 2 + 0.1, 0), Vector2(.1, 0.06))
    # _create_box_gizmo("ScaleY", Color(0.6, 1, 0.2, 1), Transform3D.IDENTITY,
    #                     ManipulationMode.SCALE, Vector3(0, MOVE_GIZMO_LEN + SCALE_GIZMO_LEN / 2 + 0.1, 0), Vector2(.1, 0.06))
    # _create_box_gizmo("ScaleZ", Color(0.5, 0.5, 1, 1), Transform3D.IDENTITY.rotated(Vector3.RIGHT, PI / 2.0),
    #                     ManipulationMode.SCALE, Vector3(0, MOVE_GIZMO_LEN + SCALE_GIZMO_LEN / 2 + 0.1, 0), Vector2(.1, 0.06))
    
    # _create_plane_gizmo("RotateX", Color(1, 0.2, 0.2, 1), Transform3D.IDENTITY.rotated(Vector3.UP, PI / 2.0))
    # _create_plane_gizmo("RotateY", Color(0.6, 1, 0.2, 1), Transform3D.IDENTITY.rotated(Vector3.RIGHT, PI / 2.0))
    # _create_plane_gizmo("RotateZ", Color(0.5, 0.5, 1, 1), Transform3D.IDENTITY)

    _create_circle_gizmo("RotateX", Color(1, 0.2, 0.2, 1), Transform3D.IDENTITY.rotated(Vector3.FORWARD, PI / 2.0))
    _create_circle_gizmo("RotateY", Color(0.6, 1, 0.2, 1), Transform3D.IDENTITY)
    _create_circle_gizmo("RotateZ", Color(0.5, 0.5, 1, 1), Transform3D.IDENTITY.rotated(Vector3.RIGHT, PI / 2.0))

func _input(event: InputEvent) -> void:
    if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT:
        if not event.is_pressed():
            # Stop dragging when the mouse button is released.
            _manipulation_mode = ManipulationMode.NONE

    if event is InputEventMouseMotion and _manipulation_mode != ManipulationMode.NONE:
        var camera := get_viewport().get_camera_3d()
        if not camera:
            return

        var mouse_pos: Vector2 = (event as InputEventMouseMotion).position
        var ray_origin := camera.project_ray_origin(mouse_pos)
        var ray_dir := camera.project_ray_normal(mouse_pos)

        var intersection_point: Variant = _drag_plane.intersects_ray(ray_origin, ray_dir)
        if intersection_point != null:
            var new_transform := _calculate_new_transform(intersection_point)
            set_gizmo_transform(new_transform)


func _process(_delta: float) -> void:
    # Check if the user has moved, rotated, or scaled the gizmo in the editor.
    if global_transform != _last_transform:
        _last_transform = global_transform
        if update_callback.is_valid():
            update_callback.call(global_transform)

func set_gizmo_transform(new_transform: Transform3D) -> void:
    """Public method to update the gizmo's position and orientation from outside."""
    global_transform = new_transform
    _last_transform = new_transform

func _create_box_gizmo(p_name: String, p_color: Color, p_transform: Transform3D,
                        p_mode: ManipulationMode, p_offset: Vector3, size: Vector2):
    var st := SurfaceTool.new()
    st.begin(Mesh.PRIMITIVE_TRIANGLES)
    var cylinder := CylinderMesh.new()
    cylinder.height = size.y
    cylinder.radial_segments = 8
    cylinder.top_radius = size.x * 0.5
    cylinder.bottom_radius = size.x * 0.5
    # Rotate the Y-aligned cylinder to point along -Z
    var cyl_transform = Transform3D.IDENTITY.translated(p_offset)
    st.append_from(cylinder, 0, cyl_transform)
    var mesh := st.commit()

    var mat := StandardMaterial3D.new()
    mat.albedo_color = p_color
    mesh.surface_set_material(0, mat)

    var mesh_instance := MeshInstance3D.new()
    mesh_instance.name = p_name
    mesh_instance.mesh = mesh
    mesh_instance.transform = p_transform

    var area := Area3D.new()
    # The area's transform is now local to the mesh instance, so it should be identity.
    area.input_event.connect(_on_gizmo_input_start.bind(mesh_instance, p_mode))
    mesh_instance.add_child(area)

    var collision_shape := CollisionShape3D.new()
    var box_shape := BoxShape3D.new()
    box_shape.size = Vector3(size.x, size.y, size.x)
    collision_shape.shape = box_shape
    collision_shape.transform = Transform3D.IDENTITY.translated(p_offset)
    area.add_child(collision_shape)
    
    add_child(mesh_instance)

func _create_plane_gizmo(p_name: String, p_color: Color, p_transform: Transform3D):
    var disc := QuadMesh.new()
    disc.size = Vector2(SCALE_GIZMO_LEN, SCALE_GIZMO_LEN)

    var mat := ShaderMaterial.new()
    mat.shader = circle_shader
    mat.set_shader_parameter("base_color", p_color)

    var mesh_instance := MeshInstance3D.new()
    mesh_instance.name = p_name
    mesh_instance.mesh = disc
    mesh_instance.transform = p_transform
    mesh_instance.mesh.surface_set_material(0, mat)

    var area := Area3D.new()
    area.input_event.connect(_on_gizmo_input_start.bind(mesh_instance, ManipulationMode.ROTATE))
    mesh_instance.add_child(area)

    var collision_shape := CollisionShape3D.new()
    var box_shape := BoxShape3D.new()
    box_shape.size = Vector3(SCALE_GIZMO_LEN, SCALE_GIZMO_LEN, 0.02)
    collision_shape.shape = box_shape
    area.add_child(collision_shape)

    add_child(mesh_instance)


func _create_circle_gizmo(p_name: String, p_color: Color, p_transform: Transform3D):
    var disc := TorusMesh.new()
    disc.rings = 16
    disc.ring_segments = 4
    disc.inner_radius = SCALE_GIZMO_LEN / 2 - .05
    disc.outer_radius = SCALE_GIZMO_LEN / 2

    var mat := ShaderMaterial.new()
    # mat.shader = circle_shader
    mat.set_shader_parameter("base_color", p_color)

    var mesh_instance := MeshInstance3D.new()
    mesh_instance.name = p_name
    mesh_instance.mesh = disc
    mesh_instance.transform = p_transform
    mesh_instance.mesh.surface_set_material(0, mat)

    var area := Area3D.new()
    area.input_event.connect(_on_gizmo_input_start.bind(mesh_instance, ManipulationMode.ROTATE))
    mesh_instance.add_child(area)

    var collision_shape := CollisionShape3D.new()
    collision_shape.shape = disc.create_trimesh_shape()
    area.add_child(collision_shape)

    add_child(mesh_instance)


func _on_gizmo_input_start(camera: Camera3D, event: InputEvent, position: Vector3, normal: Vector3, shape_idx: int, mesh_node: MeshInstance3D, mode: ManipulationMode) -> void:
    if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT and event.is_pressed():
        var parent_area: Area3D = mesh_node.get_child(0) # Assuming the Area3D is the first child
        print(parent_area.get_parent().name, ", origin : ", parent_area.position, ", position : ", position)

        var plane_normal: Vector3
        _manipulation_mode = mode

        match mesh_node.name:
            "MoveX": _drag_axis = global_transform.basis.x
            "MoveY": _drag_axis = global_transform.basis.y
            "MoveZ": _drag_axis = global_transform.basis.z
            ###
            "ScaleZ": _drag_axis = global_transform.basis.z
            "ScaleY": _drag_axis = global_transform.basis.y
            "ScaleX": _drag_axis = global_transform.basis.x
            ###
            "RotateX": _drag_axis = global_transform.basis.x
            "RotateY": _drag_axis = global_transform.basis.y
            "RotateZ": _drag_axis = global_transform.basis.z

        if mode == ManipulationMode.MOVE or mode == ManipulationMode.SCALE:
            # Define a plane to drag along. The plane contains the drag axis and is oriented towards the camera.
            # To create a stable plane, we find a vector perpendicular to the drag axis and the camera's view axis.
            # This gives us the normal for a plane that contains the drag axis and faces the camera.
            var camera_forward := -camera.global_transform.basis.z
            plane_normal = _drag_axis.cross(camera_forward).cross(_drag_axis).normalized()
            
            # If the camera is aligned with the drag axis, the normal can be zero. Fallback to a different axis.
            if plane_normal.is_zero_approx():
                plane_normal = _drag_axis.cross(camera.global_transform.basis.x).cross(_drag_axis).normalized()
        
        elif mode == ManipulationMode.ROTATE:
            plane_normal = _drag_axis
        
        _drag_plane = Plane(plane_normal, global_transform.origin)
        var intersection: Variant = _drag_plane.intersects_ray(camera.project_ray_origin(event.position), camera.project_ray_normal(event.position))
        if intersection:
            _drag_start_position = intersection
            _drag_start_transform = global_transform
            _drag_start_vector = (intersection - global_transform.origin).normalized()

func _calculate_new_transform(p_intersection_point: Vector3) -> Transform3D:
    var new_transform := _drag_start_transform

    match _manipulation_mode:
        ManipulationMode.MOVE:
            var motion_vector := p_intersection_point - _drag_start_position
            var projected_motion_length := motion_vector.dot(_drag_axis)
            if move_snap > 0:
                projected_motion_length = snapped(projected_motion_length, move_snap)
            
            new_transform.origin = _drag_start_transform.origin + _drag_axis * projected_motion_length
        ManipulationMode.ROTATE:
            if not _drag_start_vector.is_zero_approx():
                var rotation_axis := _drag_axis.normalized()

                var current_vector := (p_intersection_point - new_transform.origin).normalized()
                
                # Project vectors onto the rotation plane to get a stable 2D angle
                var v_start_proj = (_drag_start_vector - _drag_start_vector.project(rotation_axis)).normalized()
                var v_current_proj = (current_vector - current_vector.project(rotation_axis)).normalized()
                
                if v_start_proj.is_zero_approx() or v_current_proj.is_zero_approx():
                    return new_transform

                # Calculate the angle between the start and current drag vectors
                var angle := v_start_proj.signed_angle_to(v_current_proj, rotation_axis)
                if rotate_snap > 0:
                    angle = snapped(angle, deg_to_rad(rotate_snap))
                
                # Apply the rotation to the original basis
                new_transform.basis = _drag_start_transform.basis.rotated(rotation_axis, angle)
        ManipulationMode.SCALE:
            var motion_vector := p_intersection_point - _drag_start_position
            var projected_motion_length := motion_vector.dot(_drag_axis)
            if scale_snap > 0:
                projected_motion_length = snapped(projected_motion_length, scale_snap)
            
            var scale_factor = 1.0 + projected_motion_length
            if scale_factor > 0: # Prevent negative/inverted scaling
                var scale_vector = _drag_axis.abs() * (scale_factor - 1.0) + Vector3.ONE
                new_transform = _drag_start_transform.scaled_local(scale_vector)

    return new_transform
