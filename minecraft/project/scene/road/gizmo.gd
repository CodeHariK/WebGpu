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

var _last_transform: Transform3D

var MOVE_GIZMO_LEN: float = .5
var SCALE_GIZMO_LEN: float = 1.25

@export_group("Manipulation")
@export var _manipulation_mode := ManipulationMode.NONE
var _drag_axis: Vector3
var _drag_plane := Plane()
var _drag_start_position: Vector3


func _init() -> void:
    # This ensures the gizmo runs its _process loop in the editor.
    set_process_mode(Node.PROCESS_MODE_PAUSABLE)

    _create_arrow_gizmo("MoveX", Color(1, 0.2, 0.2, 1), Transform3D().rotated(Vector3.UP, -PI / 2.0))
    _create_arrow_gizmo("MoveY", Color(0.6, 1, 0.2, 1), Transform3D().rotated(Vector3.RIGHT, PI / 2.0))
    _create_arrow_gizmo("MoveZ", Color(0.5, 0.5, 1, 1), Transform3D.IDENTITY)
    
    _create_ring_gizmo("RotateX", Color(1, 0.2, 0.2, 1), Transform3D().rotated(Vector3.UP, PI / 2.0))
    _create_ring_gizmo("RotateY", Color(0.6, 1, 0.2, 1), Transform3D.IDENTITY.rotated(Vector3.RIGHT, PI / 2.0))
    _create_ring_gizmo("RotateZ", Color(0.5, 0.5, 1, 1), Transform3D())

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

func _create_arrow_gizmo(p_name: String, p_color: Color, p_transform: Transform3D):
    var st := SurfaceTool.new()
    st.begin(Mesh.PRIMITIVE_TRIANGLES)
    var cylinder := CylinderMesh.new()
    cylinder.height = MOVE_GIZMO_LEN
    cylinder.radial_segments = 8
    cylinder.top_radius = 0.02
    cylinder.bottom_radius = 0.02
    # Rotate the Y-aligned cylinder to point along -Z
    var cyl_transform = Transform3D().rotated(Vector3.RIGHT, PI / 2.0).translated(Vector3(0, 0, -MOVE_GIZMO_LEN / 2 - SCALE_GIZMO_LEN / 2))
    st.append_from(cylinder, 0, cyl_transform)
    var cone := BoxMesh.new()
    cone.size = Vector3(0.05, 0.05, 0.05) # Base radius and height
    var cone_transform = Transform3D().translated(Vector3(0, 0, -MOVE_GIZMO_LEN - SCALE_GIZMO_LEN / 2))
    st.append_from(cone, 0, cone_transform)
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
    area.input_event.connect(_on_gizmo_input.bind(mesh_instance, ManipulationMode.MOVE))
    mesh_instance.add_child(area)

    var collision_shape := CollisionShape3D.new()
    var box_shape := BoxShape3D.new()
    box_shape.size = Vector3(0.1, 0.1, MOVE_GIZMO_LEN)
    collision_shape.shape = box_shape
    collision_shape.transform = Transform3D.IDENTITY.translated(Vector3(0, 0, -MOVE_GIZMO_LEN / 2 - SCALE_GIZMO_LEN / 2))
    area.add_child(collision_shape)
    
    add_child(mesh_instance)

func _create_ring_gizmo(p_name: String, p_color: Color, p_transform: Transform3D):
    var disc := QuadMesh.new()
    disc.size = Vector2(SCALE_GIZMO_LEN, SCALE_GIZMO_LEN)

    var shader := Shader.new()
    shader.code = """
shader_type spatial;
render_mode unshaded, cull_disabled;

uniform vec4 base_color : source_color;
uniform float ring_inner_radius = 0.48;
uniform float ring_outer_radius = 0.49;
uniform float feather = 0.01;

void fragment() {
    vec2 uv_centered = UV - vec2(0.5);
    float dist_from_center = length(uv_centered);
    
    float ring = smoothstep(ring_inner_radius - feather, ring_inner_radius, dist_from_center) -
                 smoothstep(ring_outer_radius, ring_outer_radius + feather, dist_from_center);

    ALBEDO = base_color.rgb;
    ALPHA = base_color.a * ring;
}
"""
    var mat := ShaderMaterial.new()
    mat.shader = shader
    mat.set_shader_parameter("base_color", p_color)

    var mesh_instance := MeshInstance3D.new()
    mesh_instance.name = p_name
    mesh_instance.mesh = disc
    mesh_instance.transform = p_transform
    mesh_instance.mesh.surface_set_material(0, mat)

    var area := Area3D.new()
    area.input_event.connect(_on_gizmo_input.bind(mesh_instance, ManipulationMode.ROTATE))
    mesh_instance.add_child(area)

    var collision_shape := CollisionShape3D.new()
    var box_shape := BoxShape3D.new()
    box_shape.size = Vector3(SCALE_GIZMO_LEN, SCALE_GIZMO_LEN, 0.02)
    collision_shape.shape = box_shape
    area.add_child(collision_shape)

    add_child(mesh_instance)

func _on_gizmo_input(camera: Camera3D, event: InputEvent, position: Vector3, normal: Vector3, shape_idx: int, mesh_node: MeshInstance3D, mode: ManipulationMode) -> void:
    if event is InputEventMouseButton and event.button_index == MOUSE_BUTTON_LEFT and event.is_pressed():
        var parent_area: Area3D = mesh_node.get_child(0) # Assuming the Area3D is the first child
        print(parent_area.get_parent().name, ", origin : ", parent_area.position, ", position : ", position)

        _manipulation_mode = ManipulationMode.ROTATE
        
        var plane_normal: Vector3
        _manipulation_mode = mode

        if mode == ManipulationMode.MOVE:
            match mesh_node.name:
                "MoveY": _drag_axis = global_transform.basis.y
                "MoveZ": _drag_axis = global_transform.basis.z
                "MoveX": _drag_axis = global_transform.basis.x

            # Define a plane to drag along. The plane contains the drag axis and is oriented towards the camera.
            # To create a stable plane, we find a vector perpendicular to the drag axis and the camera's view axis.
            # This gives us the normal for a plane that contains the drag axis and faces the camera.
            var camera_forward := -camera.global_transform.basis.z
            plane_normal = _drag_axis.cross(camera_forward).cross(_drag_axis).normalized()
            
            # If the camera is aligned with the drag axis, the normal can be zero. Fallback to a different axis.
            if plane_normal.is_zero_approx():
                plane_normal = _drag_axis.cross(camera.global_transform.basis.x).cross(_drag_axis).normalized()
        
        _drag_plane = Plane(plane_normal, global_transform.origin)
        var intersection_point: Variant = _drag_plane.intersects_ray(camera.project_ray_origin(event.position), camera.project_ray_normal(event.position))
        if intersection_point != null:
            _drag_start_position = intersection_point

        elif mode == ManipulationMode.ROTATE:
            match mesh_node.name:
                "RotateY": _drag_axis = global_transform.basis.y
                "RotateX": _drag_axis = global_transform.basis.x
                "RotateZ": _drag_axis = global_transform.basis.z
            plane_normal = _drag_axis
        
            _drag_plane = Plane(plane_normal, global_transform.origin)
            _drag_start_position = position


func _calculate_new_transform(p_intersection_point: Vector3) -> Transform3D:
    var new_transform := global_transform
    var motion_offset := p_intersection_point - global_transform.origin

    match _manipulation_mode:
        ManipulationMode.MOVE:
            var projected_motion := motion_offset.project(_drag_axis)
            new_transform.origin += projected_motion
        ManipulationMode.ROTATE:
            var target_dir := (p_intersection_point - global_transform.origin).normalized()
            if not target_dir.is_zero_approx() and not _drag_axis.is_zero_approx():
                # Get a reference vector in the rotation plane (e.g., the gizmo's local X-axis).
                var reference_vector: Vector3
                if _drag_axis.is_equal_approx(new_transform.basis.x):
                    # If rotating around X, use Y as the reference.
                    reference_vector = new_transform.basis.y
                else:
                    # Otherwise, use X as the reference.
                    reference_vector = new_transform.basis.x
                # Project it onto the rotation plane to ensure it's perpendicular to the drag axis.
                reference_vector = reference_vector - reference_vector.project(_drag_axis)

                # Calculate the angle between the reference and the target direction.
                var angle := reference_vector.signed_angle_to(target_dir, _drag_axis)
                new_transform.basis = new_transform.basis.rotated(_drag_axis, angle)

    return new_transform
